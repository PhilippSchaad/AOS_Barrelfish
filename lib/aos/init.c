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
#include <aos/domain_network_interface.h>
#include <nameserver.h>

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;
static bool network_terminal_domain;
static uint16_t terminal_domain = 0;
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

/* Write to the terminal */
static bool sending = false;
static size_t terminal_write(const char *buf, size_t len)
{
    if (len) {
        if (init_domain) {
            // If we are the init domain, write via syscall.
            sys_print(buf, len);
        } else if(network_terminal_domain){
            if(sending) return 0;
            // domain belonging to a network terminal report back to the network
            struct network_print_message* message = malloc(sizeof(struct network_print_message));
            message->message_type = NETWORK_PRINT_MESSAGE;
            message->payload_size = 200;
            memset(message->payload, 0x0, 200);
            snprintf(message->payload, len+5, "->: %s\0", buf);
            sending = true;
            CHECK(aos_rpc_send_message_to_process(aos_rpc_get_init_channel(), terminal_domain, disp_get_core_id(), message, sizeof(struct network_print_message)));
            free(message);
            sending = false;
        }else {
            // If we are NOT init, write via rpc -> init.
            char *buf_cpy = malloc((len + 1) * sizeof(char));
            snprintf(buf_cpy, len + 1, "%s\0", buf);
            CHECK(aos_rpc_send_string(aos_rpc_get_serial_channel(), buf_cpy));
            free(buf_cpy);
        }
    }
    return len;
}

static size_t terminal_read(char *buf, size_t len)
{
    if (len) {
        if (init_domain) {
            sys_getchar(buf);
        } else {
            CHECK(aos_rpc_serial_getchar(aos_rpc_get_serial_channel(), buf));
        }
    }
    return 1;
}

/* Set libc function pointers */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and
    // what we need for that
    // TODO: change these to use the user-space serial driver if possible
    _libc_terminal_read_func = terminal_read;
    _libc_terminal_write_func = terminal_write;
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
    DBG(VERBOSE, "We're gonna register ourself with the procman now\n");
    // The first cmdline argument holds our process domain name
    char *domain_name = (char *) params->argv[0];
    aos_rpc_process_register(aos_rpc_get_process_channel(), domain_name);

    DBG(VERBOSE, "Registration completed, running main now\n");


    // check if we run from the network terminal
    if(params->argc != 1 && strcmp(params->argv[params->argc-1],"__terminalized") == 0){
        --params->argc;
        network_terminal_domain = true;
        // get the port from the nameserver
         struct nameserver_query nsq;
        nsq.tag = nsq_name;
        nsq.name = "Network_Terminal";
        struct nameserver_info *nsi;
        CHECK(lookup(&nsq,&nsi));
        if(nsi != NULL) {
            //printf("NSI with core id %u, pid %u, and propcount %d\n", nsi->coreid, strtoul(nsi->props->prop_attr, NULL, 0), nsi->nsp_count);
        }else{
            printf("Requested spawn by network terminal, but seems that no network terminal is registered\n");
            return EXIT_FAILURE;
        }
        terminal_domain = strtoul(nsi->props->prop_attr, NULL, 0);
    }

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
