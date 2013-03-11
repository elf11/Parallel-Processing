CC=gcc
CCFLAGS = -Wall -O3 -g
OMPFLAGS = -fopenmp
.PHONY : clean

all: serial paralel

serial: serial.o
	$(CC) $(CCFLAGS) $^ -o $@

serial.o: serial.c
	$(CC) $(CCFLAGS) $^ -c -o $@

paralel: paralel.o
	$(CC) $(OMPFLAGS) $(CCFLAGS) $^ -o $@

paralel.o: paralel.c
	$(CC) $(OMPFLAGS) $(CCFLAGS) $^ -c -o $@

clean:
	rm -rf *.o 
