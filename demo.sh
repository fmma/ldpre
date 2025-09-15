#!/bin/bash

echo "=== File Tee LD_PRELOAD Demo ==="
echo

# Create demo directory and work in it
echo "0. Setting up demo directory..."
mkdir -p demo
cd demo
echo "✓ Working in ./demo directory"
echo

# Build the library
echo "1. Building the file_tee.so library..."
make -C .. clean && make -C ..
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi
cp ../file_tee.so .
echo "✓ Build successful"
echo

# Create a test program that writes to multiple files
echo "2. Creating test program..."
cat > test_writer.c << 'EOF'
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main() {
    // Test fwrite
    printf("Testing fwrite...\n");
    FILE *f1 = fopen("/tmp/test1.txt", "w");
    fprintf(f1, "Hello from fwrite!\n");
    fwrite("Binary data: \x01\x02\x03\x04\n", 1, 18, f1);
    fclose(f1);
    
    // Test write syscall  
    printf("Testing write syscall...\n");
    int fd = open("test2.txt", O_CREAT | O_WRONLY, 0644);
    const char *msg = "Hello from write syscall!\n";
    write(fd, msg, strlen(msg));
    close(fd);
    
    // Test append mode
    printf("Testing append mode...\n");
    FILE *f2 = fopen("/tmp/test1.txt", "a");
    fprintf(f2, "Appended line!\n");
    fclose(f2);
    
    printf("All tests completed!\n");
    return 0;
}
EOF

gcc -o test_writer test_writer.c
echo "✓ Test program created"
echo

# Run without LD_PRELOAD first
echo "3. Running test program WITHOUT LD_PRELOAD..."
rm -rf tee_copies test1.txt test2.txt
./test_writer
echo "Files created:"
ls -la test*.txt 2>/dev/null | grep -v "^total"
echo

# Run with LD_PRELOAD
echo "4. Running test program WITH LD_PRELOAD..."
rm -rf tee_copies test1.txt test2.txt
LD_PRELOAD=./file_tee.so ./test_writer
echo "Files created:"
ls -la test*.txt 2>/dev/null | grep -v "^total"
echo

# Show tee copies
echo "5. Checking tee copies..."
if [ -d "tee_copies" ]; then
    echo "Tee directory structure:"
    find tee_copies -type f
    echo
    echo "Contents of tee copies:"
    for file in $(find tee_copies -name "*.tee"); do
        echo "--- $file ---"
        cat "$file"
        echo
    done
else
    echo "No tee_copies directory found!"
fi

# Compare original vs tee
echo "6. Comparing original files vs tee copies..."
for orig in test1.txt test2.txt; do
    if [ -f "$orig" ]; then
        tee_file="tee_copies$(pwd)/${orig}.tee"
        if [ -f "$tee_file" ]; then
            echo "Comparing $orig with its tee copy:"
            if diff -q "$orig" "$tee_file" >/dev/null; then
                echo "✓ Files are identical"
            else
                echo "✗ Files differ!"
                echo "Original:"
                cat "$orig"
                echo "Tee copy:"
                cat "$tee_file"
            fi
        else
            echo "Tee copy not found for $orig"
        fi
        echo
    fi
done


echo
echo "=== Demo completed ==="
echo "All demo files are in the ./demo directory"
echo "To use with any program: LD_PRELOAD=./file_tee.so <your_program>"
echo "Tee copies will be saved in ./tee_copies/<full_path>.tee"