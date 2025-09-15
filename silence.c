//
// gcc -shared -fPIC -o silence_ahbmrt.so silence_ahbmrt.c -ldl
//
// cp silence_ahbmrt.so ../src/cxl-gitlab.ssi.samsung.com/ahbm/ahbmpt/
//
// Run with LD_PRELOAD=./silence_ahbmrt.so, e.g:
//
// LD_PRELOAD=./silence_ahbmrt.so python example.py
//

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <string.h>
#include <stdarg.h>

typedef int (*vprintf_t)(const char *fmt, va_list ap);
typedef int (*vfprintf_t)(FILE *stream, const char *fmt, va_list ap);

int printf(const char *fmt, ...)
{
    static vprintf_t real_vprintf = NULL;
    static vfprintf_t real_vfprintf = NULL;
    if (!real_vprintf)
    {
        real_vprintf = (vprintf_t)dlsym(RTLD_NEXT, "vprintf");
        real_vfprintf = (vfprintf_t)dlsym(RTLD_NEXT, "vfprintf");
    }

    void *caller = __builtin_return_address(0);
    Dl_info info;
    if (dladdr(caller, &info) && info.dli_fname)
    {
        const char *silence_lib = getenv("SILENCE");
        if (silence_lib && strstr(info.dli_fname, silence_lib))
        {
            va_list args;
            va_start(args, fmt);
            char logfile_name[256];
            snprintf(logfile_name, sizeof(logfile_name), "%s.log", silence_lib);
            FILE *logfile = fopen(logfile_name, "a");
            int ret = real_vfprintf(logfile, fmt, args);
            fclose(logfile);
            va_end(args);
            return ret;
        }
    }

    va_list args;
    va_start(args, fmt);
    int ret = real_vprintf(fmt, args);
    va_end(args);
    return ret;
}
