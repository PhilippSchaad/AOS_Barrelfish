#ifndef LIB_URPC_H
#define LIB_URPC_H

#include <lib_procman.h>
#include "init_headers/lib_urpc2.h"

void urpc_master_init_and_run(void *urpc_vaddr);
void urpc_slave_init_and_run(void);

// Does not delete the string. TODO: version which does?
void urpc_sendstring(char *str);
void urpc_spawn_process(char *name);
void urpc_init_mem_alloc(struct bootinfo *p_bi);
bool urpc_ram_is_initalized(void);
void urpc2_send_and_receive(struct urpc2_data (*func)(void *data), void *payload, char type, struct event_closure callback_when_done);

void urpc_register_process(char *str);

#endif /* LIB_URPC_H */
