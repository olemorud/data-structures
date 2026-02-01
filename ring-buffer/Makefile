CC := clang

%.o: %.c
	$(CC) $(CFLAGS) -c $<

tests/%: tests/%.c
	$(CC) -I. $(CFLAGS) -o $@ $<

demo: demo-ring-buffer.o ring-buffer-int-128.o
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: tests clean

tests: tests/ring-buffer-test-1 tests/ring-buffer-test-2

clean: 
	rm tests/ring-buffer-test-1 tests/ring-buffer-test-2 demo demo-ring-buffer.o ring-buffer-int-128.o
