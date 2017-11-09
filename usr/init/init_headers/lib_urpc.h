#ifndef LIB_URPC_H
#define LIB_URPC_H

void urpc_master_init_and_run(void *urpc_vaddr);
void urpc_slave_init_and_run(void);

// Does not delete the string. TODO: version which does?
void urpc_sendstring(char *str);
void urpc_sendram(struct capref *cap);
void urpc_spawn_process(char *name);
bool urpc_ram_is_initalized(void);

#endif /* LIB_URPC_H */
