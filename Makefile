CC = gcc
CFLAGS = -Wall -Wextra -fPIC -shared -O2 -g
LDFLAGS = -ldl -lpthread

TARGET = file_tee.so
SOURCE = file_tee.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
	rm -rf tee_copies test_output.txt
	rm -rf demo

test: $(TARGET)
	@echo "Building test program..."
	@echo '#include <stdio.h>\nint main() {\n    FILE *f = fopen("test_output.txt", "w");\n    fprintf(f, "Hello World\\n");\n    fclose(f);\n    return 0;\n}' > test_program.c
	@gcc -o test_program test_program.c
	@echo "Running test with file_tee.so..."
	@LD_PRELOAD=./$(TARGET) ./test_program
	@echo "Original file:"
	@cat test_output.txt 2>/dev/null || echo "Original file not found"
	@echo "Tee copy:"
	@find tee_copies -name "*.tee" -exec cat {} \; 2>/dev/null || echo "Tee copy not found"
	@rm -f test_program test_program.c

install: $(TARGET)
	cp $(TARGET) /usr/local/lib/
	ldconfig

uninstall:
	rm -f /usr/local/lib/$(TARGET)
	ldconfig