
CXX=openmpicxx
CC=openmpicc

CFLAGS=-std=gnu99 -Wall -Werror
CXXFLAGS=-std=gnu99 -Wall -Werror

all: example.o

clean:
	rm -f *.o
