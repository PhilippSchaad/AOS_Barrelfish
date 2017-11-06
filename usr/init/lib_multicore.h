#ifndef LIB_MULTICORE_H
#define LIB_MULTICORE_H

#include <aos/aos.h>
#include <aos/coreboot.h>
#include <aos/kernel_cap_invocations.h>

errval_t wake_core(coreid_t core_id, coreid_t current_core_id);

#endif /* LIB_MULTICORE_H */
