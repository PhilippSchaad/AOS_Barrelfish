#include <turtleback.h>

static char *consolidate_args(int argc, char **argv) {
    char *prog_name = argv[0];
    int prog_name_length = strlen(prog_name);
    int bin_invocation_length = prog_name_length;

    char *bin_invocation = malloc((bin_invocation_length + 1) * sizeof(char));
    strncpy(bin_invocation, prog_name, prog_name_length);
    bin_invocation[prog_name_length] = '\0';

    for (int i = 1; i < argc; i++) {
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

    return bin_invocation;
}

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

    char *bin_invocation = consolidate_args(argc - 2, &argv[2]);

    domainid_t pid;

    CHECK(aos_rpc_process_spawn(aos_rpc_get_init_channel(), bin_invocation,
                                core, &pid));

    if (pid == UINT32_MAX)
        printf("Unable to find program %s\n", argv[2]);

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

void shell_memtest(int argc, char **argv)
{
    if (argc != 2) {
        printf("Invalid number of arguments..\n");
        printf("Usage: %s\n", MEMTEST_USAGE);
        return;
    }

    int size_mb = atoi(argv[1]);
    if (size_mb < 0 || size_mb > 1000) {
        printf("The size you entered is invalid..\n");
        printf("Hint: Your system may not have this much memory\n");
        printf("Please enter a size in the range of [0..1000] MBs\n");
        return;
    }

    int size = size_mb * 1024 * 1024;

    printf("\033[95mRunning memtest on %d MB region..\033[0m\n", size_mb);
    char *array = malloc(size * sizeof(char));
    if (!array) {
        printf("\033[31mFailed to allocate memory, aborting\033[0m\n");
        return;
    }
    printf("\033[95mWriting to region..\033[0m\n", size);
    for (int i = 0; i < size; i++)
        array[i] = 'x';
    printf("\033[95mDone writing\033[0m\n", size);
    printf("\033[95mChecking written values..\033[0m\n", size);
    int errs = 0;
    for (int i = 0; i < size; i++)
        if (array[i] != 'x')
            errs++;
    printf("\033[95mDone\033[0m\n", size);

    free(array);

    if (errs == 0)
        printf("\033[32mSUCCESS\033[0m\n");
    else
        printf("\033[31mEncountered %d erroneous values!\033[0m\n", errs);
}

void shell_run_testsuite(int argc, char **argv)
{
    CHECK(aos_rpc_run_testsuite(aos_rpc_get_init_channel()));
}

void shell_time(int argc, char **argv)
{
    if (argc < 2) {
        printf("Too few arguments supplied..\n");
        printf("Usage: %s\n", TIME_USAGE);
        return;
    }

    uint32_t cycles;

    bool handled = false;

    // Try matcing against builtins.
    for (int i = 0; shell_builtins[i].cmd != NULL; i++) {
        if (strcmp(argv[1], shell_builtins[i].cmd) == 0) {
            if (strcmp(shell_builtins[i].cmd, "time") == 0) {
                printf("We can't time the time command, that's stupid.\n");
                printf("Try a different one. Type `help' to see a list of "
                       "possible commands.\n");
                return;
            }
            handled = true;
            reset_cycle_counter();
            shell_builtins[i].invoke(argc - 1, &argv[1]);
            cycles = get_cycle_count();
            break;
        }
    }

    if (!handled) {
        char *bin_invocation = consolidate_args(argc - 1, &argv[1]);

        domainid_t pid;
        reset_cycle_counter();
        CHECK(aos_rpc_process_spawn(get_init_rpc(), bin_invocation,
                                    disp_get_core_id(), &pid));

        if (pid != UINT32_MAX) {
            handled = true;

            CHECK(aos_rpc_process_await_completion(get_init_rpc(), pid));
        }

        cycles = get_cycle_count();

        free(bin_invocation);
    }

    if (!handled) {
        printf("The command %s was not found..\n", argv[1]);
        printf("Type `help' to see a list of available commands\n");
        return;
    }

    double seconds = (double) cycles / CLOCK_FREQUENCY;
    printf("\nTime: %.4lfs\n", seconds);
}
