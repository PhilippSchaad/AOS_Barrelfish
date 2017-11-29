#include <turtleback.h>

void shell_invalid_command(char *cmd)
{
    printf("%s: command not found\n", cmd);
}

void shell_help(int argc, char **argv)
{
    printf("TurtleBack SHELL, version %d.%d.%d (%s-%d.%d.%d)\n",
           TURTLEBACK_VERSION_MAJOR, TURTLEBACK_VERSION_MINOR,
           TURTLEBACK_PATCH_LEVEL, OS_NAME, OS_VERSION_MAJOR, OS_VERSION_MINOR,
           OS_PATCH_LEVEL);
    printf("These shell commands are defined internally. "
           "Type `help' to see this list.\n");
    for (int i = 0; shell_builtins[i].cmd != NULL; i++) {
        printf(" %-10s - %s\n", shell_builtins[i].cmd,
               shell_builtins[i].help_text);
    }
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
