all: bloom

bloom: bloom.o hash-config.o test.o
	cc -o bloom -Wall -pedantic bloom.o hash-config.o test.o

bloom.o: bloom.c bloom.h
	cc -o bloom.o -Wall -pedantic -ansi -c bloom.c

hash-config.o: hash-config.c hash-config.h
	cc -c hash-config.c

test.o: test.c bloom.h hash-config.h
	cc -o test.o -Wall -pedantic -ansi -c test.c
