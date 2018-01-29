#ifndef __GRADING_OPTIONS_H
#define __GRADING_OPTIONS_H

struct grading_options {
    /* Should we run an allocation test in init and halt? (nothing else will
     * work, as we've taken all the RAM). Option "g:ira0=<size>" */
    bool init0_ram_alloc;
    size_t init0_ram_alloc_size;

    /* Same test as above, but on core 1. Option "g:ira1=<size>" */
    bool init1_ram_alloc;
    size_t init1_ram_alloc_size;

    /* Simple spawn test from init.1. Option "g:sps1" */
    bool init1_simple_spawn;

    /* Spawn 'memtest' directly from init on core 0/1. */
    bool init0_memtest;
    bool init1_memtest;
};

/* Checks to see if the given argument is a grading option (starts with "g:")
 * and, if so, sets the appropriate option values and returns true.  Otherwise
 * returns false. */
bool grading_handle_arg(struct grading_options *options, const char *arg);

#endif /* __GRADING_OPTIONS_H */
