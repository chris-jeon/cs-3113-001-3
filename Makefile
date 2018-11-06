CC=gcc

all: zformat

zformat: zformat.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zformat.c oufs_lib_support.c vdisk.c -o zformat

zformat.o: zformat.c
	$(CC) -c zformat.c

oufs_lib_support.o: oufs_lib_support.c
	$(CC) -c oufs_lib_support.c

vdisk.o: vdisk.c
	$(CC) -c vdisk.c

clean:
	rm *.o zformat
