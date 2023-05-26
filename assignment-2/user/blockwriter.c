#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

int main(int argc, char *argv[]) {

	int fd;
	int numOfBlocks;
	int numOfBytes;
	char* filename;
    
	if(strcmp(argv[1], "-h") == 0) {
		printf("1. help (-h) prikazuje help meni\n2. output-file  (-o) FILENAME postavlja ime za novokreiranu datoteku FILENAME\n3. blocks (-b) BLOCKS postavlja broj blokova za ispisivanje na BLOCKS.\n");
		exit();
	}
	
	if(argc < 2) {
		numOfBlocks = 150;
		filename = "long";
	}
	else {
		filename = argv[2];
		numOfBlocks = atoi(argv[4]);
	}

	if((fd = open(filename, 0x200 | 0x001)) < 0) {
		printf("cat: cannot open %s\n", argv[1]);
		exit();
	}

    numOfBytes = numOfBlocks * BSIZE;
    char buffer[BSIZE];
    memset(buffer, 'A', BSIZE);

    while (numOfBytes > 0)
    {
        int bytesToWrite;
		if (numOfBytes > BSIZE) {
    		bytesToWrite = BSIZE;
		} 
		else {
    		bytesToWrite = numOfBytes;
		}
        write(fd, buffer, bytesToWrite);
        numOfBytes -= bytesToWrite;
        printf("Ispisan blok: %d\n", (int)(numOfBytes / BSIZE));
    }
    exit();
}
