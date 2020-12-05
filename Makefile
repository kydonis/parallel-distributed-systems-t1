CC=gcc-9
CFLAGS=-O3

default: all

all: main

main: v4_omp.c
	$(CC) $(CFLAGS) -o $@ $^ readmtx.c mmio.c coo2csc.c timer.c arrayutils.c -fopenmp

clean:
	rm -f main

test:
	./main ./data/s12.mtx
	./main ./data/bcsstm07.mtx
	./main ./data/G1.mtx
