# p4 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
PPLIBS = -lpp -L.

ppexec:	p4_tester libpp.a ppexec.c
	$(CC) $(DEBUGFLAGS) -o ppexec ppexec.c

libpp.a: libpp.c
	$(CC) $(DEBUGFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o

p4_tester: libpp.a
	$(CC) $(DEBUGFLAGS) -o p4_tester p4_tester.c $(PPLIBS)

clean:
	rm -f ppexec *.o *.a p4_tester
