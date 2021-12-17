#
# Makefile for hello_world_pmem example in C++
#
PROGS = hello_world_pmem
CXXFLAGS = -g -Wall -Werror -std=gnu++11

all: $(PROGS)

hello_world_pmem: LIBS = -lpmem -lpmemobj -pthread

hello_world_pmem: hello_world_pmem.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LIBS)

clean:
	$(RM) *.o core

clobber: clean
	$(RM) $(PROGS)

.PHONY: all clean clobber
