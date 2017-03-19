# p2 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
PPLIBS = -lpp -L.

ppexec:	libpp.a ppexec.c
	$(CC) $(CFLAGS) -o ppexec ppexec.c

libpp.a: libpp.c
	$(CC) $(CFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o

clean:
	rm -f ppexec *.o *.a t?
