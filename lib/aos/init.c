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
static struct aos_rpc init_rpc;

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
        CHECK(aos_rpc_kill_me(aos_rpc_get_process_channel()));
        errval_t err = cap_delete(cap_dispatcher);
        if (err_is_fail(err)) {
            sys_print("revoking dispatcher failed in _Exit, spinning!\n", 100);
            while (1) {}
        }
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
/*
static size_t serial_channel_write(const char *buf, size_t len)
{
    if (len) {
        aos_rpc_send_string(aos_rpc_get_serial_channel(), buf);
        return len;
    }
    return 0;
}
*/

/*
static size_t dummy_terminal_read(char *buf, size_t len)
{
    debug_printf("terminal read NYI! returning %d characters read\n", len);
    return len;
}
*/

/* Set libc function pointers */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and
    // what we need for that
    // TODO: change these to use the user-space serial driver if possible
    //_libc_terminal_read_func = dummy_terminal_read;
    if (init_domain){
        _libc_terminal_write_func = syscall_terminal_write;
    } else {
        _libc_terminal_write_func = syscall_terminal_write;
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

    // Register ourselves with init
    aos_rpc_init(&init_rpc);
    ram_alloc_set(NULL);

    // Register ourselves in the process manager.
    debug_printf("We're gonna register ourself with the procman now\n");

    // The first cmdline argument holds our process domain name
    char *domain_name = (char *) params->argv[0];
    // TODO: This might need to be truncated if the process is spawned
    //       with path (eg /local/usr/bin/process instead of process)..
    aos_rpc_process_register(aos_rpc_get_process_channel(), domain_name);

    debug_printf("Registration completed, running main now\n");
    
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
