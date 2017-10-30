/**
 * \file
 * \brief Barrelfish library initialization.
 */

/*
 * Copyright (c) 2007-2016, ETH Zurich.
 * Copyright (c) 2014, HP Labs.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <stdio.h>
#include <aos/aos.h>
#include <aos/dispatch.h>
#include <aos/curdispatcher_arch.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/aos_rpc.h>
#include <barrelfish_kpi/domain_params.h>
#include "threads_priv.h"
#include "init.h"

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;

extern size_t (*_libc_terminal_read_func)(char *, size_t);
extern size_t (*_libc_terminal_write_func)(const char *, size_t);
extern void (*_libc_exit_func)(int);
extern void (*_libc_assert_func)(const char *, const char *, const char *, int);

void libc_exit(int);

void libc_exit(int status)
{
    // Use spawnd if spawned through spawnd
    if(disp_get_domain_id() == 0) {
        /*
        errval_t err = cap_revoke(cap_dispatcher);
        if (err_is_fail(err)) {
            sys_print("revoking dispatcher failed in _Exit, spinning!", 100);
            while (1) {}
        }
        */
        errval_t err = cap_delete(cap_dispatcher);
        if (err_is_fail(err)) {
            sys_print("revoking dispatcher failed in _Exit, spinning!\n", 100);
            while (1) {}
        }
        // XXX: Leak all other domain allocations
    } else {
        debug_printf("libc_exit NYI!\n");
    }

    thread_exit(status);
    // If we're not dead by now, we wait
    sys_print("revoking dispatcher failed in _Exit, spinning!\n", 100);
    while (1) {}
}

static void libc_assert(const char *expression, const char *file,
                        const char *function, int line)
{
    char buf[512];
    size_t len;

    /* Formatting as per suggestion in C99 spec 7.2.1.1 */
    len = snprintf(buf, sizeof(buf), "Assertion failed on core %d in %.*s: %s,"
                   " function %s, file %s, line %d.\n",
                   disp_get_core_id(), DISP_NAME_LEN,
                   disp_name(), expression, function, file, line);
    sys_print(buf, len < sizeof(buf) ? len : sizeof(buf));
}

// use this function for serial domain
static size_t syscall_terminal_write(const char *buf, size_t len)
{
    if (len) {
        return sys_print(buf, len);
    }
    return 0;
}

// use this function on all non serial domains
static size_t serial_channel_write(const char *buf, size_t len)
{
    if (len) {
        aos_rpc_send_string(aos_rpc_get_serial_channel(), buf);
        return len;
    }
    return 0;
}


static size_t dummy_terminal_read(char *buf, size_t len)
{
    debug_printf("terminal read NYI! returning %d characters read\n", len);
    return len;
}

/* Set libc function pointers */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and
    // what we need for that
    // TODO: change these to use the user-space serial driver if possible
    _libc_terminal_read_func = dummy_terminal_read;
    if (init_domain){
        _libc_terminal_write_func = syscall_terminal_write;
    } else {
        _libc_terminal_write_func = serial_channel_write;
    }
    _libc_exit_func = libc_exit;
    _libc_assert_func = libc_assert;
    /* morecore func is setup by morecore_init() */

    // XXX: set a static buffer for stdout
    // this avoids an implicit call to malloc() on the first printf
    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IOLBF, sizeof(buf));
    static char ebuf[BUFSIZ];
    setvbuf(stderr, ebuf, _IOLBF, sizeof(buf));
}

/** \brief Initialise libbarrelfish.
 *
 * This runs on a thread in every domain, after the dispatcher is setup but
 * before main() runs.
 */
errval_t barrelfish_init_onthread(struct spawn_domain_params *params)
{
    errval_t err;

    // do we have an environment?
    if (params != NULL && params->envp[0] != NULL) {
        extern char **environ;
        environ = params->envp;
    }

    // Init default waitset for this dispatcher
    struct waitset *default_ws = get_default_waitset();
    waitset_init(default_ws);

    // Initialize ram_alloc state
    ram_alloc_init();
    /* All domains use smallcn to initialize */
    err = ram_alloc_set(ram_alloc_fixed);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }

    err = paging_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_VSPACE_INIT);
    }

    err = slot_alloc_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_SLOT_ALLOC_INIT);
    }

    err = morecore_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_MORECORE_INIT);
    }

    lmp_endpoint_init();

    // init domains only get partial init
    if (init_domain) {
        return SYS_ERR_OK;
    }

    // TODO MILESTONE 3: register ourselves with init
    struct aos_rpc rpc;
    aos_rpc_init(&rpc);
    ram_alloc_set(NULL);

    /* TODO MILESTONE 3: now we should have a channel with init set up and can
     * use it for the ram allocator */
    // Testing some RPCs:
    //DBG(RELEASE, "Testing PUTCHAR RPC:\n");
    //CHECK(aos_rpc_serial_putchar(&rpc, 'I'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 't'));
    //CHECK(aos_rpc_serial_putchar(&rpc, ' '));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'i'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 's'));
    //CHECK(aos_rpc_serial_putchar(&rpc, ' '));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'w'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'o'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'r'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'k'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'i'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'n'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'g'));
    //CHECK(aos_rpc_serial_putchar(&rpc, '!'));
    //CHECK(aos_rpc_serial_putchar(&rpc, '\n'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'n'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'e'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'w'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'l'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'i'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'n'));
    //CHECK(aos_rpc_serial_putchar(&rpc, 'e'));
    //char greeting[6] = {'H', 'e', 'l', 'l', 'o', '\0'};
    //CHECK(aos_rpc_send_string(&rpc, greeting));
    ////try long message
    //char longmessage[101];
    //for(uint8_t i=0;i<99;++i){
    //    if(i%2){
    //        longmessage[i] = 'a';
    //    } else {
    //        longmessage[i] = 'b';
    //    }
    //}
    //longmessage[99] = 'e';
    //longmessage[100] = '\0';
    //CHECK(aos_rpc_send_string(&rpc, longmessage));
    //CHECK(aos_rpc_serial_putchar(&rpc, '\n'));

    //DBG(RELEASE, "Testing RAM RPC:\n");
    //size_t reqsize = 100;
    //size_t retsize;
    //struct capref frame_1;
    //CHECK(aos_rpc_get_ram_cap(&rpc, reqsize, BASE_PAGE_SIZE, &frame_1, &retsize));
    //DBG(RELEASE, "We asked for %u and got %u memory\n", reqsize, retsize);

    //reqsize = 1024 * 1024 * 10;
    //struct capref frame_2;
    //CHECK(aos_rpc_get_ram_cap(&rpc, reqsize, BASE_PAGE_SIZE, &frame_2, &retsize));
    //DBG(RELEASE, "We asked for %u and got %u memory\n", reqsize, retsize);

    //DBG(RELEASE, "Testing SEND-NUMBER RPC:\n");
    //CHECK(aos_rpc_send_number(&rpc, 1995));

    //printf("test the serial server print\n");

    // right now we don't have the nameservice & don't need the terminal
    // and domain spanning, so we return here
    return SYS_ERR_OK;
}


/**
 *  \brief Initialise libbarrelfish, while disabled.
 *
 * This runs on the dispatcher's stack, while disabled, before the dispatcher is
 * setup. We can't call anything that needs to be enabled (ie. cap invocations)
 * or uses threads. This is called from crt0.
 */
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg);
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg)
{
    init_domain = init_dom_arg;
    disp_init_disabled(handle);
    thread_init_disabled(handle, init_dom_arg);
}
