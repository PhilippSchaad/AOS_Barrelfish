#ifndef LIB_MULTICORE_H
#define LIB_MULTICORE_H

#include <aos/aos.h>
#include <aos/coreboot.h>
#include <aos/kernel_cap_invocations.h>
#include <barrelfish_kpi/arm_core_data.h>
#include <spawn/multiboot.h>

errval_t wake_core(coreid_t core_id, coreid_t current_core_id,
                   struct bootinfo *bootinfo);

errval_t create_urpc_frame(void** buf, size_t bytes);

#endif /* LIB_MULTICORE_H */
