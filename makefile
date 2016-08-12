CC := clang
CFLAGS := -O3 -Wall -Wextra
LDFLAGS := -ldl

all:	mtrace.so unpack

mtrace.o:	mtrace.c mtrace.h makefile
	$(CC) $< -o $@ $(CFLAGS) -fPIC -c

mtrace.so:	mtrace.o
	$(CC) $< -o $@ $(LDFLAGS) -shared

unpack.o:	unpack.c mtrace.h makefile
	$(CC) $< -o $@ $(CFLAGS) -c

unpack:	unpack.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	rm -f *.o mtrace.so unpack
