CC=gcc
CFLAGS=-c -Wall -O2
AS=yasm -f elf

all:
	$(CC) $(CFLAGS) inject.c
	$(CC) inject.o -o inject -ldl

	$(CC) $(CFLAGS) -fPIC event.c dlwrap.c
	$(LD) -Bshareable -o event.so event.o dlwrap.o -lpthread

clean:
	rm -rf *.o




