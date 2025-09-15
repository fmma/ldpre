#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>

#define MAX_FDS 1024
#define TEE_DIR "./tee_copies"

static char fd_paths[MAX_FDS][PATH_MAX];
static pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static int initialized = 0;

static int (*real_open)(const char *pathname, int flags, ...);
static int (*real_openat)(int dirfd, const char *pathname, int flags, ...);
static int (*real_creat)(const char *pathname, mode_t mode);
static int (*real_close)(int fd);
static ssize_t (*real_write)(int fd, const void *buf, size_t count);
static ssize_t (*real_pwrite)(int fd, const void *buf, size_t count, off_t offset);
static ssize_t (*real_writev)(int fd, const struct iovec *iov, int iovcnt);
static FILE *(*real_fopen)(const char *pathname, const char *mode);
static FILE *(*real_freopen)(const char *pathname, const char *mode, FILE *stream);
static size_t (*real_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
static int (*real_fclose)(FILE *stream);

static void init_real_functions()
{
    if (initialized)
        return;

    real_open = dlsym(RTLD_NEXT, "open");
    real_openat = dlsym(RTLD_NEXT, "openat");
    real_creat = dlsym(RTLD_NEXT, "creat");
    real_close = dlsym(RTLD_NEXT, "close");
    real_write = dlsym(RTLD_NEXT, "write");
    real_pwrite = dlsym(RTLD_NEXT, "pwrite");
    real_writev = dlsym(RTLD_NEXT, "writev");
    real_fopen = dlsym(RTLD_NEXT, "fopen");
    real_freopen = dlsym(RTLD_NEXT, "freopen");
    real_fwrite = dlsym(RTLD_NEXT, "fwrite");
    real_fclose = dlsym(RTLD_NEXT, "fclose");

    mkdir(TEE_DIR, 0755);
    initialized = 1;
}

static void store_fd_path(int fd, const char *pathname)
{
    if (fd < 0 || fd >= MAX_FDS || !pathname)
        return;

    pthread_mutex_lock(&fd_mutex);
    char resolved[PATH_MAX];
    if (realpath(pathname, resolved))
    {
        strncpy(fd_paths[fd], resolved, PATH_MAX - 1);
        fd_paths[fd][PATH_MAX - 1] = '\0';
    }
    else
    {
        strncpy(fd_paths[fd], pathname, PATH_MAX - 1);
        fd_paths[fd][PATH_MAX - 1] = '\0';
    }
    pthread_mutex_unlock(&fd_mutex);
}

static void clear_fd_path(int fd)
{
    if (fd < 0 || fd >= MAX_FDS)
        return;

    pthread_mutex_lock(&fd_mutex);
    fd_paths[fd][0] = '\0';
    pthread_mutex_unlock(&fd_mutex);
}

static char *get_fd_path(int fd)
{
    if (fd < 0 || fd >= MAX_FDS)
        return NULL;

    static __thread char path_buf[PATH_MAX];
    pthread_mutex_lock(&fd_mutex);

    if (fd_paths[fd][0] != '\0')
    {
        strncpy(path_buf, fd_paths[fd], PATH_MAX - 1);
        path_buf[PATH_MAX - 1] = '\0';
        pthread_mutex_unlock(&fd_mutex);
        return path_buf;
    }

    pthread_mutex_unlock(&fd_mutex);

    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);

    ssize_t len = readlink(proc_path, path_buf, PATH_MAX - 1);
    if (len > 0)
    {
        path_buf[len] = '\0';
        return path_buf;
    }

    return NULL;
}

static int should_tee_file(const char *path)
{
    if (!path)
        return 0;
    
    const char *ext = strrchr(path, '.');
    if (!ext)
        return 0;
    
    ext++; // Skip the dot
    
    return (strcmp(ext, "ll") == 0 ||
            strcmp(ext, "c") == 0 ||
            strcmp(ext, "cpp") == 0 ||
            strcmp(ext, "py") == 0);
}

static void write_to_tee(const char *original_path, const void *buf, size_t count)
{
    if (!original_path || count == 0)
        return;
    
    if (!should_tee_file(original_path))
        return;

    char tee_path[PATH_MAX * 2];
    snprintf(tee_path, sizeof(tee_path), "%s%s", TEE_DIR, original_path);

    char *dir_end = strrchr(tee_path, '/');
    if (dir_end)
    {
        *dir_end = '\0';
        char mkdir_cmd[PATH_MAX * 2 + 20];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", tee_path);
        if (system(mkdir_cmd) == -1)
        {
            /* mkdir failed, but continue anyway */
        }
        *dir_end = '/';
    }

    int tee_fd = real_open(tee_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (tee_fd >= 0)
    {
        real_write(tee_fd, buf, count);
        real_close(tee_fd);
    }
}

int open(const char *pathname, int flags, ...)
{
    init_real_functions();

    int fd;
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        fd = real_open(pathname, flags, mode);
    }
    else
    {
        fd = real_open(pathname, flags);
    }

    if (fd >= 0 && (flags & (O_WRONLY | O_RDWR)))
    {
        store_fd_path(fd, pathname);
    }

    return fd;
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    init_real_functions();

    int fd;
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        fd = real_openat(dirfd, pathname, flags, mode);
    }
    else
    {
        fd = real_openat(dirfd, pathname, flags);
    }

    if (fd >= 0 && (flags & (O_WRONLY | O_RDWR)))
    {
        if (pathname[0] == '/')
        {
            store_fd_path(fd, pathname);
        }
        else
        {
            char full_path[PATH_MAX];
            char dir_path[PATH_MAX];

            if (dirfd == AT_FDCWD)
            {
                if (getcwd(dir_path, sizeof(dir_path)))
                {
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, pathname);
                    store_fd_path(fd, full_path);
                }
            }
            else
            {
                char *dir = get_fd_path(dirfd);
                if (dir)
                {
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, pathname);
                    store_fd_path(fd, full_path);
                }
            }
        }
    }

    return fd;
}

int creat(const char *pathname, mode_t mode)
{
    init_real_functions();

    int fd = real_creat(pathname, mode);
    if (fd >= 0)
    {
        store_fd_path(fd, pathname);
    }

    return fd;
}

int close(int fd)
{
    init_real_functions();

    clear_fd_path(fd);
    return real_close(fd);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    init_real_functions();

    ssize_t result = real_write(fd, buf, count);

    if (result > 0 && fd != STDOUT_FILENO && fd != STDERR_FILENO)
    {
        char *path = get_fd_path(fd);
        if (path)
        {
            write_to_tee(path, buf, result);
        }
    }

    return result;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    init_real_functions();

    ssize_t result = real_pwrite(fd, buf, count, offset);

    if (result > 0 && fd != STDOUT_FILENO && fd != STDERR_FILENO)
    {
        char *path = get_fd_path(fd);
        if (path)
        {
            write_to_tee(path, buf, result);
        }
    }

    return result;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    init_real_functions();

    ssize_t result = real_writev(fd, iov, iovcnt);

    if (result > 0 && fd != STDOUT_FILENO && fd != STDERR_FILENO)
    {
        char *path = get_fd_path(fd);
        if (path)
        {
            for (int i = 0; i < iovcnt; i++)
            {
                if (iov[i].iov_len > 0)
                {
                    write_to_tee(path, iov[i].iov_base, iov[i].iov_len);
                }
            }
        }
    }

    return result;
}

FILE *fopen(const char *pathname, const char *mode)
{
    init_real_functions();

    FILE *fp = real_fopen(pathname, mode);
    if (fp && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')))
    {
        int fd = fileno(fp);
        if (fd >= 0)
        {
            store_fd_path(fd, pathname);
        }
    }

    return fp;
}

FILE *freopen(const char *pathname, const char *mode, FILE *stream)
{
    init_real_functions();

    int old_fd = fileno(stream);
    if (old_fd >= 0)
    {
        clear_fd_path(old_fd);
    }

    FILE *fp = real_freopen(pathname, mode, stream);
    if (fp && pathname && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')))
    {
        int fd = fileno(fp);
        if (fd >= 0)
        {
            store_fd_path(fd, pathname);
        }
    }

    return fp;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    init_real_functions();

    size_t result = real_fwrite(ptr, size, nmemb, stream);

    if (result > 0 && stream != stdout && stream != stderr)
    {
        int fd = fileno(stream);
        if (fd >= 0)
        {
            char *path = get_fd_path(fd);
            if (path)
            {
                write_to_tee(path, ptr, result * size);
            }
        }
    }

    return result;
}

int fclose(FILE *stream)
{
    init_real_functions();

    int fd = fileno(stream);
    if (fd >= 0)
    {
        clear_fd_path(fd);
    }

    return real_fclose(stream);
}