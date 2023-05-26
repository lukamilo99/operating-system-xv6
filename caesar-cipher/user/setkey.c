#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int isDigit(char input) {
	if(input >= '0' && input <= '9')
		return 1;
	else
		return 0;
}

int main(int argc, char *argv[]) {
    
	if (argc != 2) {
        printf(2, "Usage: %s [0|1]\n", argv[0]);
        exit();
    }

	if(isDigit(argv[1][0])) {
		int value = atoi(argv[1]);
    	int ret = setkey(value);
	}
	else if(strcmp(argv[1], "-h") == 0) {
		printf("1. help (-h) prikazuje help meni\n2. secret (-s) uzima ključ sa STDIN-a i pritom sakriva ključ uz pomoć sistemskog poziva setecho\n");
	}
	else {
		printf("Unesite kljuc:\n");
		setecho(0);
		char input[10];
		read(0, input, sizeof(input));
		int value = atoi(input);
    	int ret = setkey(value);
		setecho(1);
		printf("\n");
	}
    exit();
}


