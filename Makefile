all: zformat zinspect

.c.o:
	gcc -c $< -o $@

zformat: zformat.c
	gcc vdisk.c oufs_lib_support.c zformat.c -o zformat
zinspect: zinspect.c
	gcc oufs_lib_support.c -o zinspect

clean: 
	rm ./zformat ./zinspect