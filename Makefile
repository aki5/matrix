CFLAGS=-O3 -fomit-frame-pointer -D_GNU_SOURCE
PROGS=\
	sort\
	matrix\
	matrix2\

OFILES=\
	os.o\
	cube.o\
	matrix.o\
	matrix2.o\

all: $(PROGS)

sort: sort.o os.o cube.o
	$(CC) $(CFLAGS) -o $@ $^

matrix: matrix.o os.o cube.o
	$(CC) $(CFLAGS) -o $@ $^

matrix2: matrix2.o os.o cube.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(PROGS) *.o

$(OFILES): os.h cube.h
