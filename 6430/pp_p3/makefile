# p3 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
PPLIBS = -lpp -L.

ppexec:	p3_tester libpp.a ppexec.c
	$(CC) $(CFLAGS) -o ppexec ppexec.c

libpp.a: libpp.c
	$(CC) $(CFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o

p3_tester: libpp.a
	$(CC) $(DEBUGFLAGS) -o p3_tester p3_tester.c $(PPLIBS)

clean:
	rm -f ppexec *.o *.a p3_tester
