CC := clang
CFLAGS := -O3 -Wall -Wextra -pthread
LDFLAGS :=
LIBS := -ldl -lpthread

all:	mtrace.so unpack mtrace_test

mtrace.o:	mtrace.c mtrace.h makefile
	$(CC) $< -o $@ $(CFLAGS) -fPIC -c

mtrace.so:	mtrace.o
	$(CC) $< -o $@ $(LDFLAGS) $(LIBS) -shared

unpack.o:	unpack.c mtrace.h makefile
	$(CC) $< -o $@ $(CFLAGS) -c

unpack:	unpack.o
	$(CC) $< -o $@ $(LDFLAGS) $(LIBS)

mtrace_test:	mtrace_test.c mtrace.h mtrace.c makefile
	$(CC) mtrace_test.c mtrace.c -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o mtrace.so unpack mtrace_test
