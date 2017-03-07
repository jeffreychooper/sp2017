CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS= -std=gnu99 -g -D DEBUG
LIBS = 

proj: proj.c
	$(CC) $(CFLAGS) -o proj proj.c $(LIBS)

debug:
	$(CC) $(DEBUGFLAGS) -o proj proj.c $(LIBS)

clean:
	rm -f proj *.o
