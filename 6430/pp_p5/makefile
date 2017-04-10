# p5 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
PPLIBS = -lpp -L.

ppexec:	P5TEST1 P5TEST2 P5TEST3 libpp.a ppexec.c
	$(CC) $(CFLAGS) -o ppexec ppexec.c

libpp.a: libpp.c
	$(CC) $(CFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o

P5TEST1: libpp.a
	$(CC) $(CFLAGS) -o P5TEST1 P5TEST1.c $(PPLIBS)

P5TEST2: libpp.a
	$(CC) $(CFLAGS) -o P5TEST2 P5TEST2.c $(PPLIBS)
	
P5TEST3: libpp.a
	$(CC) $(CFLAGS) -o P5TEST3 P5TEST3.c $(PPLIBS)

debug:
	$(CC) $(DEBUGFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o
	$(CC) $(DEBUGFLAGS) -o P5TEST1 P5TEST1.c $(PPLIBS)
	$(CC) $(DEBUGFLAGS) -o P5TEST2 P5TEST2.c $(PPLIBS)
	$(CC) $(DEBUGFLAGS) -o P5TEST3 P5TEST3.c $(PPLIBS)
	$(CC) $(DEBUGFLAGS) -o ppexec ppexec.c

clean:
	rm -f ppexec *.o *.a P5TEST?
