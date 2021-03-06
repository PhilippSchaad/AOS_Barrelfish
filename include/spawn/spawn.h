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
    char * binary_name;                 ///< Name of the binary
    struct capref l1_cnode;             ///< Process's L1 CNode
    struct cnoderef l2_cnode_list[ROOTCN_SLOTS_USER];   ///< Foreign L2 CNodes
    struct capref dispatcher;           ///< The dispatcher
    struct capref process_l1_pt;        ///< The page table of the new process
    struct capref spawned_disp_memframe;///< Dispatcher memory in spawned proc
    struct paging_state paging_state;   ///< New process's paging state
    genvaddr_t u_got;                   ///< Uspace address of the GOT
    genvaddr_t entry_addr;              ///< Program entry point
    arch_registers_state_t *enabled_area;
    int next_slot;
    errval_t (*slot_callback)(struct spawninfo* si, struct capref cap);
    struct paging_frame_node *allocated_sects;
};

/// Start a child process by binary name. This fills in the spawninfo.
errval_t spawn_load_by_name(char * binary_name, struct spawninfo * si);

#endif /* _INIT_SPAWN_H_ */
