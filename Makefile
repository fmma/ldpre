CC = gcc
CFLAGS = -Wall -Wextra -fPIC -shared -O2 -g
LDFLAGS = -ldl -lpthread

TARGETS = file_tee.so silence.so
SOURCES = file_tee.c silence.c

.PHONY: all clean test

all: $(TARGETS)

file_tee.so: file_tee.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

silence.so: silence.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
	rm -rf tee_copies test_output.txt
	rm -rf demo

test: file_tee.so
	@echo "Building test program..."
	@echo '#include <stdio.h>\nint main() {\n    FILE *f = fopen("test_output.txt", "w");\n    fprintf(f, "Hello World\\n");\n    fclose(f);\n    return 0;\n}' > test_program.c
	@gcc -o test_program test_program.c
	@echo "Running test with file_tee.so..."
	@LD_PRELOAD=./file_tee.so ./test_program
	@echo "Original file:"
	@cat test_output.txt 2>/dev/null || echo "Original file not found"
	@echo "Tee copy:"
	@find tee_copies -name "*.tee" -exec cat {} \; 2>/dev/null || echo "Tee copy not found"
	@rm -f test_program test_program.c

install: $(TARGETS)
	cp $(TARGETS) /usr/local/lib/
	ldconfig

uninstall:
	rm -f /usr/local/lib/file_tee.so /usr/local/lib/silence.so
	ldconfig