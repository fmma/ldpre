# File Tee LD_PRELOAD Library

An LD_PRELOAD library that intercepts file write operations and creates tee copies of all written data.

## Features

- Intercepts low-level write operations (`write`, `pwrite`, `writev`)
- Intercepts high-level stdio operations (`fwrite`, `fprintf`)
- Tracks file descriptor to pathname mapping
- Creates tee copies in `./tee_copies/` directory
- Thread-safe operation
- Preserves original file behavior

## Building

```bash
make
```

## Usage

### Basic Usage
```bash
LD_PRELOAD=./file_tee.so your_program
```

### Examples
```bash
# Tee all writes from a simple program
LD_PRELOAD=./file_tee.so echo "hello" > test.txt

# Tee all writes from cp command
LD_PRELOAD=./file_tee.so cp source.txt dest.txt

# Tee all writes from a compiler
LD_PRELOAD=./file_tee.so gcc -o program program.c
```

## Output Structure

Tee copies are saved to:
```
./tee_copies/<absolute_path_to_original_file>.tee
```

For example, if a program writes to `/home/user/test.txt`, the tee copy will be at:
```
./tee_copies/home/user/test.txt.tee
```

## Testing

Run the included demo:
```bash
./demo.sh
```

Or run the quick test:
```bash
make test
```

## How It Works

1. **Function Interception**: Uses `dlsym(RTLD_NEXT, ...)` to get original function pointers
2. **Path Tracking**: Maps file descriptors to absolute paths using:
   - Function parameter tracking for `open`/`fopen` calls
   - `/proc/self/fd/N` resolution for unknown file descriptors
3. **Tee Writing**: On each write operation:
   - Calls the original function first
   - Writes the same data to a tee file in `./tee_copies/`
4. **Thread Safety**: Uses pthread mutexes for concurrent access

## Intercepted Functions

- `open`, `openat`, `creat` - File opening
- `close`, `fclose` - Cleanup tracking
- `write`, `pwrite`, `writev` - Low-level writes  
- `fopen`, `freopen` - High-level file opening
- `fwrite` - High-level writes

## Limitations

- Only tracks files opened for writing (`O_WRONLY`, `O_RDWR`, `w`, `a`, `+`)
- Skips stdout/stderr to avoid recursion
- Directory creation uses `system()` call (could be optimized)
- Limited to `MAX_FDS` (1024) concurrent file descriptors

## Security Considerations

This tool is intended for defensive security analysis, debugging, and system monitoring. It allows transparent monitoring of file write operations without modifying applications.