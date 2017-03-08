# p3 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
LIBS =

p3:	p3.c
	$(CC) $(CFLAGS) -o p3 p3.c $(LIBS)

debug: p3.c
	$(CC) $(DEBUGFLAGS) -o p3 p3.c $(LIBS)

clean:
	rm -f p3 *.o
