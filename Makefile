all: zformat zinspect zfilez zmkdir zrmdir

.c.o:
	gcc -c $< -o $@

zformat: zformat.c
	gcc vdisk.c oufs_lib_support.c zformat.c -o zformat
zinspect: zinspect.c
	gcc vdisk.c oufs_lib_support.c zinspect.c -o zinspect
zfilez: zfilez.c
	gcc vdisk.c oufs_lib_support.c zfilez.c -o zfilez
zmkdir: zmkdir.c
	gcc vdisk.c oufs_lib_support.c zmkdir.c -o zmkdir
zrmdir: zrmdir.c
	gcc vdisk.c oufs_lib_support.c zrmdir.c -o zrmdir

clean: 
	rm ./zformat ./zinspect ./zfilez ./zmkdir ./zrmdir
