all: unsec unsquash

unsec: unsec.o compress.o

unsquash: unsquash.o compress.o

clean:
	rm -f unsec unsquash *.o
