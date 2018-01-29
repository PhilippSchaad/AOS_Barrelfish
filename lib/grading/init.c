#include <stdio.h>

#include <aos/aos.h>
#include <aos/capabilities.h>
#include <aos/ram_alloc.h>
#include <grading.h>
#include <spawn/spawn.h>

#include "io.h"
#include "options.h"
#include "state.h"

static void
stop(void) {
    while(1);
}

static void
grading_init_ramalloc(size_t size) {
    errval_t err;

    grading_printf("init ram_alloc() test, %zuB\n", size);

    /* Allocate a frame of the given size. */
    struct capref frame_cap;
    size_t actual_size;
    err= frame_alloc(&frame_cap, size, &actual_size);
    if(!err_is_ok(err)) {
        grading_test_fail("ira", "frame_alloc(): %s\n", err_getstring(err));
        stop();
    }
    if(actual_size < size) {
        grading_test_fail("ira", "frame_alloc() returned only %zuB.\n",
                          actual_size);
        stop();
    }

    /* Map the frame. */
    void *vbase;
    struct frame_identity id;
    err = frame_identify(frame, &id);
    if(err_is_fail(err)){
        return err;
    }
    err= paging_map_frame_attr(get_current_paging_state(),
                               &vbase, id.bytes, frame_cap,
                               VREGION_FLAGS_READ_WRITE, NULL, NULL);
    if(!err_is_ok(err)) {
        grading_test_fail("ira", "paging_map_frame_complete(): %x\n",
                          err_getstring(err));
        stop();
    }

    /* Check that it's actually there - write and verify. */
    size_t nwords= size / sizeof(uint32_t);
    volatile uint32_t *words= vbase;
    for(size_t i= 0; i < nwords; i++) words[i]= (uint32_t)i;
    for(size_t i= 0; i < nwords; i++) {
        if(words[i] != i) {
            grading_test_fail("ira", "reread failed at offset %zu.\n",
                              i * sizeof(uint32_t));
            stop();
        }
    }

    grading_test_pass("ira", "\n");
    stop();
}

void
grading_early_init(void) {
    if(grading_options.init0_ram_alloc && grading_coreid == 0) {
        grading_init_ramalloc(grading_options.init0_ram_alloc_size);
    }

    if(grading_options.init1_ram_alloc && grading_coreid == 1) {
        grading_init_ramalloc(grading_options.init1_ram_alloc_size);
    }
}

#define NSPAWN 8
#define SIMPLE_CHILD "simplechild"

void lmp_recv_handler(struct aos_chan *ac);

static void
simple_spawn(void) {
    errval_t err;

    grading_printf("Spawning one %s\n", SIMPLE_CHILD);

    struct spawninfo *si = malloc(sizeof(struct spawninfo));
    if(!si) {
        grading_test_fail("sps", "malloc(): failed\n");
        return;
    }

    uint32_t child_pid;
    err= spawn_load_by_name(SIMPLE_CHILD, si, &child_pid);
    if(!err_is_ok(err)) {
        grading_test_fail("sps", "spawn_load_by_name(): %s\n",
                          err_getstring(err));
        free(si);
        return;
    }

    err= aos_chan_set_recv_handler(&si->aos_chan, lmp_recv_handler, NULL);
    if(err_is_fail(err)) {
        grading_test_fail("sps", "aos_chan_set_recv_handler(): %s\n",
                          err_getstring(err));
        free(si);
        return;
    }

    grading_printf("Spawning %d %s\n", NSPAWN, SIMPLE_CHILD);

    struct spawninfo *c_si = malloc(NSPAWN * sizeof(struct spawninfo));
    if(!si) {
        grading_test_fail("sps", "malloc(): failed\n");
        return;
    }

    uint32_t child_pids[NSPAWN];
    for(size_t i= 0; i < NSPAWN; i++) {
        err= spawn_load_by_name(SIMPLE_CHILD, &c_si[i], &child_pids[i]);
        if(!err_is_ok(err)) {
            grading_test_fail("sps", "spawn_load_by_name(): %s\n",
                              err_getstring(err));
            free(si);
            return;
        }

        err= aos_chan_set_recv_handler(&c_si[i].aos_chan, lmp_recv_handler,
                                       NULL);
        if(err_is_fail(err)) {
            grading_test_fail("sps", "aos_chan_set_recv_handler(): %s\n",
                              err_getstring(err));
            free(si);
            return;
        }
    }
}

static void
memtest_spawn(void) {
    errval_t err;

    grading_printf("memtest on core %u\n", (unsigned int)grading_coreid);

    struct spawninfo *si = malloc(sizeof(struct spawninfo));
    if(!si) {
        grading_test_fail("sps", "malloc(): failed\n");
        return;
    }

    uint32_t child_pid;
    err= spawn_load_by_name("memtest", si, &child_pid);
    if(!err_is_ok(err)) {
        grading_test_fail("sps", "spawn_load_by_name(): %s\n",
                          err_getstring(err));
        free(si);
        return;
    }

    err= aos_chan_set_recv_handler(&si->aos_chan, lmp_recv_handler, NULL);
    if(err_is_fail(err)) {
        grading_test_fail("sps", "aos_chan_set_recv_handler(): %s\n",
                          err_getstring(err));
        free(si);
        return;
    }
}

void
grading_late_init(void) {
    if(grading_options.init1_simple_spawn && grading_coreid == 1) {
        simple_spawn();
    }

    if(grading_options.init0_memtest && grading_coreid == 0) {
        memtest_spawn();
    }

    if(grading_options.init1_memtest && grading_coreid == 1) {
        memtest_spawn();
    }
}
