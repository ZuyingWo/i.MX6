all: capturer_mmap


capturer_mmap: capturer_mmap.o
	 $(CROSS_COMPILE)gcc -static $(CFLAGS) -O2  -o capturer_mmap capturer_mmap.c


clean:
	rm -f capturer_mmap
	rm -f capturer_mmap.o

