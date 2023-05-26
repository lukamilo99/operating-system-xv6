#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf(2, "Usage: %s [0|1]\n", argv[0]);
        exit();
    }

    int do_echo = atoi(argv[1]);
    int ret = setecho(do_echo);

    exit();
}
