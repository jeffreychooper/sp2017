# p4 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
LIBS =

p4:	p4.c
	$(CC) $(CFLAGS) -o p4 p4.c $(LIBS)

debug: p4.c
	$(CC) $(DEBUGFLAGS) -o p4 p4.c $(LIBS)

clean:
	rm -f p4 *.o
