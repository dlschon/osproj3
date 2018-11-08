all: zformat zinspect zfilez

.c.o:
	gcc -c $< -o $@

zformat: zformat.c
	gcc vdisk.c oufs_lib_support.c zformat.c -o zformat
zinspect: zinspect.c
	gcc vdisk.c oufs_lib_support.c zinspect.c -o zinspect
zfilez: zfilez.c
	gcc vdisk.c oufs_lib_support.c zfilez.c -o zfilez

clean: 
	rm ./zformat ./zinspect ./zfilez
