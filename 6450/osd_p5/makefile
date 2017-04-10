# p5 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
LIBS =

p5:	p5.c
	$(CC) $(CFLAGS) -o p5 p5.c $(LIBS)

debug: p5.c
	$(CC) $(DEBUGFLAGS) -o p5 p5.c $(LIBS)

clean:
	rm -f p5 *.o
