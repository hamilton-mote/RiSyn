all: rethos

rethos: rethos.o
	$(CC) -O3 -Wall rethos.o -lrt -o rethos

rethos.o: rethos.c
	$(CC) -O3 -Wall -c rethos.c -o rethos.o

clean:
	rm -f rethos rethos.o
