CC=mpicc
.PHONY : clean

all: paralel

paralel: paralel.o
	$(CC) $^ -lm -o $@

paralel.o: paralel.c
	$(CC) $^ -lm -c -o $@

clean:
	rm -rf *.o 
