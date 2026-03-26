CFLAGS := -Wall

all: examples-O0.s examples-O2.s

%-O0.s: %.c
	gcc $(CFLAGS) $< -S -o $@

%-O2.s: %.c
	gcc $(CFLAGS) $< -O2 -S -o $@

clean:
	rm -f $(OBJS) *.s

.PHONY: all clean
