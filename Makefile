CC=gcc
SOURCES=zformat zinspect zmkdir zfilez zrmdir

all: $(SOURCES)

zrmdir: zrmdir.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zrmdir.c oufs_lib_support.c vdisk.c -o zrmdir

zrmdir.o: zrmdir.c
	$(CC) -c zrmdir.c

zfilez: zfilez.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zfilez.c oufs_lib_support.c vdisk.c -o zfilez

zfilez.o: zfilez.c
	$(CC) -c zfilez.c

zinspect: zinspect.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zinspect.c oufs_lib_support.c vdisk.c -o zinspect

zinspect.o: zinspect.c
	$(CC) -c zinspect.c

zformat: zformat.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zformat.c oufs_lib_support.c vdisk.c -o zformat

zformat.o: zformat.c
	$(CC) -c zformat.c

zmkdir: zmkdir.o oufs_lib_support.o vdisk.o
	$(CC) -Wall zmkdir.c oufs_lib_support.c vdisk.c -o zmkdir

zmkdir.o: zmkdir.c
	$(CC) -c zmkdir.c

oufs_lib_support.o: oufs_lib_support.c
	$(CC) -c oufs_lib_support.c

vdisk.o: vdisk.c
	$(CC) -c vdisk.c

clean:
	rm *.o $(SOURCES) 
