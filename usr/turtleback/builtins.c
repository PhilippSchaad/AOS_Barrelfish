#include <builtins.h>

void shell_invalid_command(char *cmd)
{
    printf("%s: command not found\n", cmd);
}

void shell_invalid_option(char *opt)
{
}

void shell_help(int argc, char **argv)
{
}

void shell_clear(int argc, char **argv)
{
    printf("\033[2J\033[1;1H");
}

void shell_echo(int argc, char **argv)
{
    for(int i = 1; i < argc; i++) {
        printf(argv[i]);
        if (i + 1 < argc)
            printf(" ");
    }
    printf("\n");
}
