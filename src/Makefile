CC = cc

all: bloom-whitelist

bloom-whitelist: bloom-whitelist.o bloom.o bloom-test.o
	$(CC) -o bloom-whitelist -Wall -pedantic bloom-whitelist.o bloom.o bloom-test.o

bloom-whitelist.o: bloom-whitelist.c bloom-whitelist.h constants.h
	$(CC) -o bloom-whitelist.o -Wall -pedantic -ansi -c bloom-whitelist.c

bloom.o: bloom.c bloom.h
	$(CC) -o bloom.o -Wall -pedantic -ansi -c bloom.c

bloom-test.o: bloom-test.c bloom-whitelist.h
	$(CC) -o bloom-test.o -Wall -pedantic -ansi -c bloom-test.c
