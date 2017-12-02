#include <turtleback.h>

void shell_invalid_command(char *cmd)
{
    printf("%s: command not found\n", cmd);
}

void shell_help(int argc, char **argv)
{
    if (argc > 1) {
        for (int i = 0; shell_builtins[i].cmd != NULL; i++) {
            if (strcmp(argv[1], shell_builtins[i].cmd) == 0) {
                printf("Usage: %s\n", shell_builtins[i].usage);
                return;
            }
        }
        printf("help: %s not found\n", argv[1]);
        printf("try `help' for a list of supported commands\n");
        printf("Usage: %s\n", HELP_USAGE);
    } else {
        printf("TurtleBack SHELL, version %d.%d.%d (%s-%d.%d.%d)\n",
               TURTLEBACK_VERSION_MAJOR, TURTLEBACK_VERSION_MINOR,
               TURTLEBACK_PATCH_LEVEL, OS_NAME, OS_VERSION_MAJOR,
               OS_VERSION_MINOR, OS_PATCH_LEVEL);
        printf("These shell commands are defined internally. "
               "Type `help' to see this list.\n");
        printf("Type `help name' to find out more about the function "
               "`name'.\n");
        for (int i = 0; shell_builtins[i].cmd != NULL; i++) {
            printf(" %-10s - %s\n", shell_builtins[i].cmd,
                   shell_builtins[i].help_text);
        }
    }
}

void shell_clear(int argc, char **argv)
{
    printf("\033[2J\033[1;1H");
    fflush(stdout);
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

void shell_oncore(int argc, char **argv)
{
    if (argc < 3) {
        printf("Too few arguments supplied..\n");
        printf("Usage: %s\n", ONCORE_USAGE);
        return;
    }

    int core = atoi(argv[1]);

    if (core != 0 && core != 1) {
        printf("This system only has two cores, use `0' or `1'\n");
        printf("Usage: %s\n", ONCORE_USAGE);
        return;
    }

    char *prog_name = argv[2];
    int prog_name_length = strlen(prog_name);
    int bin_invocation_length = prog_name_length;

    char *bin_invocation = malloc((bin_invocation_length + 1) * sizeof(char));
    strncpy(bin_invocation, prog_name, prog_name_length);
    bin_invocation[prog_name_length] = '\0';

    for (int i = 3; i < argc; i++) {
        int arg_length = strlen(argv[i]);

        // + 2 for space and null terminator.
        int new_invoc_length = bin_invocation_length + arg_length + 2;
        bin_invocation = realloc(bin_invocation, new_invoc_length);
        char *prev_invocation = malloc((bin_invocation_length + 1) *
                sizeof(char));
        strncpy(prev_invocation, bin_invocation, bin_invocation_length);
        prev_invocation[bin_invocation_length] = '\0';

        snprintf(bin_invocation, new_invoc_length, "%s %s", prev_invocation,
                 argv[i]);

        free(prev_invocation);
        bin_invocation_length = new_invoc_length;
    }

    domainid_t pid;

    CHECK(aos_rpc_process_spawn(aos_rpc_get_init_channel(), bin_invocation,
                                core, &pid));

    if (pid == UINT32_MAX)
        printf("Unable to find program %s\n", prog_name);

    free(bin_invocation);
}

void shell_ps(int argc, char **argv)
{
    CHECK(aos_rpc_print_process_list(aos_rpc_get_init_channel()));
}

void shell_led(int argc, char **argv)
{
    CHECK(aos_rpc_led_toggle(aos_rpc_get_init_channel()));
}
