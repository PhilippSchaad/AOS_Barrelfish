/* This module contains *all* per-process state for the grading library. */

#include <stdio.h>

#include <aos/aos.h>
#include <grading.h>
#include <spawn/multiboot.h>

#include "io.h"
#include "options.h"

/*** Library State ***/
/* XXX - No global variables outside this section.  All state (e.g.
 * malloc()-ed structs) must be accessible from a root in here.  All
 * non-static variables will also be prefixed with 'grading_'. */

coreid_t grading_coreid;
struct bootinfo *grading_bootinfo;
struct grading_options grading_options;
const char *grading_proc_name;

/*** End Library State ***/

/* Constructs an argv[] list of pointers to the command-line arguments
 * retrieved from bootinfo.  The argv[] array passed in must be preallocated,
 * and of at least argc_max entries.  Returns argc, or -1 on error.  Returns
 * the intermediate buffer in 'buffer', which must be freed by the caller.
 *
 * XXX - doesn't handle quoted strings. */
static int
make_argv(struct bootinfo *bi, const char *init, int argc_max, char **argv,
          char **buffer) {
    struct mem_region *module= multiboot_find_module(bi, init);
    if(!module) {
        grading_printf("multiboot_find_module() failed\n");
        return -1;
    }

    const char *cmdline= multiboot_module_opts(module);
    if(!cmdline) {
        grading_printf("multiboot_module_opts() failed\n");
        return -1;
    }

    /* Make a copy of the command line, before we chop it up. */
    char *cmdbuf= strdup(cmdline);
    if(!cmdbuf) {
        grading_printf("strdup() failed\n");
        return -1;
    }

    /* Skip leading whitespace, and initialise the tokeniser. */
    char *saveptr= NULL;
    char *tok= strtok_r(cmdbuf, " \t", &saveptr);

    /* Split on whitespace. */
    int argc= 0;
    while(tok && argc < argc_max) {
        argv[argc]= tok;
        argc++;

        tok= strtok_r(NULL, " \t", &saveptr);
    }

    *buffer= cmdbuf;
    return argc;
}

void
grading_set_bootinfo(struct bootinfo *bi) {
    int mb_argc;
    char *mb_argv[256];
    char *buffer;

    if(!bi) grading_panic("Bootinfo pointer is null.\n");
    if(grading_bootinfo) grading_panic("Bootinfo pointer already set.\n");

    grading_bootinfo= bi;

    /* Find init's multiboot command line arguments. */
    mb_argc= make_argv(grading_bootinfo, grading_proc_name, 256,
                       mb_argv, &buffer);
    if(mb_argc < 0) {
        grading_panic("Couldn't find init's multiboot command line.\n");
    }

    /* Echo the multiboot arguments. */
    if(mb_argc < 1) grading_panic("mb_argc < 1");
    grading_printf("mb_argv = [\"%s\"", mb_argv[0]);
    for(int i= 1; i < mb_argc; i++)
        grading_printf_nb(",\"%s\"", mb_argv[i]);
    grading_printf_nb("]\n");

    /* Parse the arguments. */
    for(int i= 1; i < mb_argc; i++) {
        grading_handle_arg(&grading_options, mb_argv[i]);
    }
}

static void
grading_setup_init(int argc, char **argv) {
    errval_t err;

    /* Find the core ID. */
    err= invoke_kernel_get_core_id(cap_kernel, &grading_coreid);
    if(err_is_fail(err)) grading_panic("Couldn't get core ID.\n");
    grading_printf("Grading setup on core %u\n",
                   (unsigned int)grading_coreid);

    /* Echo the command line arguments. */
    if(argc < 1) grading_panic("argc < 1");
    grading_printf("argv = [\"%s\"", argv[0]);
    for(int i= 1; i < argc; i++) grading_printf_nb(",\"%s\"", argv[i]);
    grading_printf_nb("]\n");

    grading_proc_name= argv[0];

    /* Grab the bootinfo pointer. */
    struct bootinfo *bi= (struct bootinfo*)strtol(argv[1], NULL, 10); 
    if(bi) grading_set_bootinfo(bi);
    else {
        grading_printf("bootinfo pointer is null, waiting for"
                       " it to be provided.\n");
    }
}

static void
grading_setup_noninit(int *argc, char ***argv) {
}

void
grading_setup(int *argc, char ***argv, bool init_process) {
    if(init_process) {
        /* The setup on init won't touch the command-line arguments, just read
         * them, so no pointers. */
        grading_setup_init(*argc, *argv);
    }
    else {
        grading_setup_noninit(argc, argv);
    }
}
