all: zformat.c
	gcc vdisk.c oufs_lib_support.c -o zformat
clean: zformat.c
	rm ./zformat

