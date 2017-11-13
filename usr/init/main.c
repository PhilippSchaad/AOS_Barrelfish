/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/waitset.h>

#include <mm/mm.h>
#include <spawn/spawn.h>

#include <lib_multicore.h>
#include <lib_rpc.h>
#include <lib_urpc.h>
#include <mem_alloc.h>

#include "../tests/test.h"

coreid_t my_core_id;
struct bootinfo *bi;

__attribute__((unused))
static void dump_bootinfo(void)
{
    assert(bi != NULL);
    debug_printf("We are dumping the bootinfo of core %d:\n", my_core_id);
    debug_printf("Host MSG val:  %"PRIu64"\n", bi->host_msg);
    debug_printf("Host MSG bits: %"PRIu8"\n", bi->host_msg_bits);
    debug_printf("Regions length: %zu\n", bi->regions_length);
    debug_printf("Spawn core: %zu\n", bi->mem_spawn_core);
    debug_printf("Dumping regions:\n");
    for (int i = 0; i < bi->regions_length; i++) {
        struct mem_region reg = bi->regions[i];
        debug_printf("  Region Nr %d:\n", i);
        debug_printf("    MR_Base: %"PRIxGENPADDR"\n", reg.mr_base);
        debug_printf("    MR_Bytes: %"PRIuGENSIZE" KB\n", reg.mr_bytes / 1024);
        debug_printf("    Type: ");
        switch (reg.mr_type) {
            case RegionType_Empty:
                printf("Empty\n");
                break;
            case RegionType_RootTask:
                printf("RootTask\n");
                break;
            case RegionType_PhyAddr:
                printf("PhyAddr\n");
                break;
            case RegionType_PlatformData:
                printf("PlatformData\n");
                break;
            case RegionType_Module:
                printf("Module\n");
                break;
            case RegionType_ACPI_TABLE:
                printf("ACPI_TABLE\n");
                break;
            case RegionType_Max:
                printf("MAX\n");
                break;
            default:
                printf("UNKNOWN\n");
                break;
        }
        debug_printf("    Consumed: ");
        if (reg.mr_consumed)
            printf("True\n");
        else
            printf("False\n");
        debug_printf("    MRMOD_Size: %zu\n", reg.mrmod_size);
        debug_printf("    MRMOD_Data: %td\n", reg.mrmod_data);
        debug_printf("    MRMOD_Slot: %d\n", reg.mrmod_slot);
        if (reg.mr_type == RegionType_Module && my_core_id == 0) {
            debug_printf("    Module, printing frame identity:\n");
            struct frame_identity mod_id;
            struct capref mod = {
                .cnode = cnode_module,
                .slot = reg.mrmod_slot,
            };
            CHECK(frame_identify(mod, &mod_id));
            debug_printf("      Base: %"PRIxGENPADDR"\n", mod_id.base);
            debug_printf("      Size: %"PRIuGENSIZE"\n", mod_id.bytes);
        }
    }
    debug_printf("Dumping bootinfo done..\n");
}

int main(int argc, char *argv[])
{
    errval_t err;

    // Set the core id in the disp_priv struct.
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    // First argument contains the bootinfo location, if it's not set.
    if (my_core_id == 0) {
        bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
        if (!bi) {
            assert(my_core_id > 0);
        }
    }

    if (my_core_id == 0) {
        CHECK(initialize_ram_alloc(NULL));
        //dump_bootinfo();
    }

    // init urpc channel to 2nd core
    void *buf;
    if (my_core_id == 0) {
        CHECK(create_urpc_frame(&buf, BASE_PAGE_SIZE));
        memset(buf, 0, BASE_PAGE_SIZE);

        // wake up 2nd core
        CHECK(wake_core(1, my_core_id, bi));
    }

    // create the init ep
    CHECK(cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    // Structure to keep track of domains.
    active_domains = malloc(sizeof(struct domain_list));

    // create channel to receive child eps
    struct lmp_chan chan;
    CHECK(lmp_chan_accept(&chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    CHECK(lmp_chan_alloc_recv_slot(&chan));
    CHECK(cap_copy(cap_initep, chan.local_cap));
    CHECK(lmp_chan_register_recv(
        &chan, get_default_waitset(),
        MKCLOSURE((void *) general_recv_handler, &chan)));

    if (my_core_id == 0) {
        urpc_master_init_and_run(buf);

        urpc_init_mem_alloc(bi);

        // run tests
        struct tester t;
        init_testing(&t);
        register_memory_tests(&t);
        register_spawn_tests(&t);
        tests_run(&t);

        // urpc_spawn_process("hello");
    } else {
        urpc_slave_init_and_run();

        // Wait until we have received the URPC telling us to initialiize our
        // ram allocator and have done so successfully.
        while (!urpc_ram_is_initalized())
            ;
        //dump_bootinfo();

        /*
        struct tester t;
        init_testing(&t);
        //register_memory_tests(&t);
        register_spawn_tests(&t);
        tests_run(&t);
        */

        debug_printf("Why does it need this print to complete?\n");
    }

    printf("\n");
    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    free(active_domains);

    return EXIT_SUCCESS;
}
