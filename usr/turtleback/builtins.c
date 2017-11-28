#include <builtins.h>

void shell_echo(int argc, char **argv)
{
    printf("\n");
    for(int i = 1; i < argc; i++) {
        printf(argv[i]);
        if (i + 1 < argc)
            printf(" ");
    }
}
