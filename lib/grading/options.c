#include <stdio.h>

#include <aos/aos.h>
#include <grading.h>

#include "io.h"
#include "options.h"

bool
grading_handle_arg(struct grading_options *options, const char *arg) {
    /* We only recognise options starting with "g:" */
    if(strncmp(arg, "g:", 2)) return false;

    if(!strncmp(arg, "g:ira0", 6)) {
        char *eq_char= strchr(arg, '=');
        if(eq_char) {
            options->init0_ram_alloc= true;
            options->init0_ram_alloc_size= strtoul(eq_char+1, NULL, 0);
        }
        else {
            grading_printf("Missing size for init ram alloc test.\n");
        }
    } else if(!strncmp(arg, "g:ira1", 6)) {
        char *eq_char= strchr(arg, '=');
        if(eq_char) {
            options->init1_ram_alloc= true;
            options->init1_ram_alloc_size= strtoul(eq_char+1, NULL, 0);
        }
        else {
            grading_printf("Missing size for init.1 ram alloc test.\n");
        }
    } else if(!strcmp(arg, "g:sps1")) {
        options->init1_simple_spawn= true;
    } else if(!strcmp(arg, "g:mem0")) {
        options->init0_memtest= true;
    } else if(!strcmp(arg, "g:mem1")) {
        options->init1_memtest= true;
    } else {
        grading_printf("Unrecognised grading option: \"%s\"\n", arg);
    }

    return true;
}
