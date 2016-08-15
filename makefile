CC := clang
CXX := clang++
CFLAGS := -O3 -Wall -Wextra -pthread -std=gnu11
CXXFLAGS := -O3 -Wall -Wextra -pthread -std=c++11
LDFLAGS :=
LIBS := -ldl -lpthread

all:	mtrace.so unpack mtrace_test replay

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

replay:	replay.cc makefile
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o mtrace.so unpack mtrace_test replay
