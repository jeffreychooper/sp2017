# p1 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
LIBS =

p1:	p1.c
	$(CC) $(CFLAGS) -o p1 p1.c $(LIBS)

debug: p1.c
	$(CC) $(DEBUGFLAGS) -o p1 p1.c $(LIBS)

clean:
	rm -f p1 *.o
