# p2 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
LIBS =

p2:	p2.c
	$(CC) $(CFLAGS) -o p2 p2.c $(LIBS)

debug: p2.c
	$(CC) $(DEBUGFLAGS) -o p2 p2.c $(LIBS)

clean:
	rm -f p2 *.o
