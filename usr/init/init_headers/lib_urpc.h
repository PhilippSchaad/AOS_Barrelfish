#ifndef LIB_URPC_H
#define LIB_URPC_H

#include <lib_procman.h>

void urpc_master_init_and_run(void *urpc_vaddr);
void urpc_slave_init_and_run(void);

// Does not delete the string. TODO: version which does?
void urpc_sendstring(char *str);
void urpc_spawn_process(char *name);
void urpc_init_mem_alloc(struct bootinfo *p_bi);
bool urpc_ram_is_initalized(void);

void urpc_register_process(char *str);

#endif /* LIB_URPC_H */
