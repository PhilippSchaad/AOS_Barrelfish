/**
 * \file
 * \brief create child process library
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_SPAWN_H_
#define _INIT_SPAWN_H_

#include "aos/slot_alloc.h"
#include "aos/paging.h"

/// Information about the binary.
struct spawninfo {
    char * binary_name;                                 ///< Name of the binary
    struct capref l1_cnode;                             ///< Process's L1 CNode
    struct cnoderef l2_cnode_list[ROOTCN_SLOTS_USER];   ///< Foreign L2 CNodes
    struct capref dispatcher;                           ///< The dispatcher cap
    genvaddr_t u_base;                                  ///< Uspace base
    genvaddr_t entry_addr;                              ///< Program entry point
};

/// Start a child process by binary name. This fills in the spawninfo.
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si);

#endif /* _INIT_SPAWN_H_ */
