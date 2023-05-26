#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

int
main(int argc, char *argv[])
{
	int fd;

	if(strcmp(argv[1], "-h") == 0) {
		printf("1. help (-h) prikazuje help meni\n2. encrypt-all (-a) dekriptuje sve enkriptovane fajlove u trenutnom direktorijumu\n");
	}
	else if(strcmp(argv[1], "-a") == 0) {
		struct dirent de;
		
		if((fd = open(".", 0)) < 0){
			printf("decr: cannot open\n");
		}

		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if (de.inum == 0)
            	continue;
			int currentFile;

			if((currentFile = open(de.name, 2)) < 0) {
				printf("decr: cannot open\n");
				continue;
			}
			
			int result = decr(currentFile);
			if(result == -1) printf("Key not set\n");
			else if(result == -2) printf("T_DEV type\n");
			else if(result == -3) printf("File not encrypted\n");
			else printf("File has been decrypted\n");
			close(currentFile);
		}
		close(fd);
	}
	else {
		if((fd = open(argv[1], 2)) < 0) {
			printf("decr: cannot open %s\n", argv[1]);
			exit();
		}
		int result = decr(fd);
		if(result == -1) printf("Key not set\n");
		else if(result == -2) printf("T_DEV type\n");
		else if(result == -3) printf("File not encrypted\n");
		else printf("File has been decrypted\n");
		close(fd);
	}
	exit();
}
