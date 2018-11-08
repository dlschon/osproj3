all: zformat.c
	gcc vdisk.c oufs_lib_support.c zformat.c -o zformat
clean: zformat.c
	rm ./zformat