/**
 * \file
 * \brief Interface declaration for AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _LIB_BARRELFISH_AOS_MESSAGES_H
#define _LIB_BARRELFISH_AOS_MESSAGES_H

#include <aos/aos.h>

#define RPC_TYPE_ACK                0x0
#define RPC_TYPE_FAIL               0x1
#define RPC_TYPE_NUMBER             0x2
#define RPC_TYPE_STRING             0x3
#define RPC_TYPE_RAM                0x4
#define RPC_TYPE_PUTCHAR            0x5
#define RPC_TYPE_HANDSHAKE          0x6

struct aos_rpc {
    struct lmp_chan chan;
    bool init;
};

/**
 * \brief send a number over the given channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val);

/**
 * \brief send a string over the given channel
 */
errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string);

/**
 * \brief request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes, size_t align,
                             struct capref *retcap, size_t *ret_bytes);

/**
 * \brief get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);

/**
 * \brief send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

/**
 * \brief Request process manager to start a new process
 * \arg name the name of the process that needs to be spawned (without a
 *           path prefix)
 * \arg newpid the process id of the newly spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid);

/**
 * \brief Get name of process with id pid.
 * \arg pid the process id to lookup
 * \arg name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name);

/**
 * \brief Get process ids of all running processes
 * \arg pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \arg pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count);

/**
 * \brief Gets a capability to device registers
 * \param rpc  the rpc channel
 * \param paddr physical address of the device
 * \param bytes number of bytes of the device memory
 * \param frame returned devframe
 */
errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc, lpaddr_t paddr, size_t bytes,
                                struct capref *frame);
/**
 * \brief Initialize given rpc channel.
 * TODO: you may want to change the inteface of your init function, depending
 * on how you design your message passing code.
 */
errval_t aos_rpc_init(struct aos_rpc *rpc);


/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void);

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void);

/**
 * \brief Returns the channel to the process manager
 */
struct aos_rpc *aos_rpc_get_process_channel(void);

/**
 * \brief Returns the channel to the serial console
 */
struct aos_rpc *aos_rpc_get_serial_channel(void);

#endif // _LIB_BARRELFISH_AOS_MESSAGES_H
