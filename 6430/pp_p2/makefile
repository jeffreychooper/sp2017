# p2 makefile
CC = gcc
CFLAGS = -std=gnu99
DEBUGFLAGS = -std=gnu99 -g -D DEBUG
PPLIBS = -lpp -L.

ppexec:	t1 t2 t3 t4 t5 t6 libpp.a ppexec.c
	$(CC) $(CFLAGS) -o ppexec ppexec.c

t1: libpp.a
	$(CC) $(CFLAGS) -o t1 t1.c $(PPLIBS)

t2: libpp.a
	$(CC) $(CFLAGS) -o t2 t2.c $(PPLIBS)

t3: libpp.a
	$(CC) $(CFLAGS) -o t3 t3.c $(PPLIBS)

t4: libpp.a
	$(CC) $(CFLAGS) -o t4 t4.c $(PPLIBS)

t5:	libpp.a
	$(CC) $(CFLAGS) -o t5 t5.c $(PPLIBS)

t6: libpp.a
	$(CC) $(CFLAGS) -o t6 t6.c $(PPLIBS)

libpp.a: libpp.c
	$(CC) $(CFLAGS) -c -o libpp.o libpp.c
	ar rvf libpp.a libpp.o

clean:
	rm -f ppexec *.o *.a t?
