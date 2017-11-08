//
// Created by sae on 08/11/17.
//

#ifndef GROUPD_LIB_URPC_H_H
#define GROUPD_LIB_URPC_H_H

#endif //GROUPD_LIB_URPC_H_H
void urpc_master_init_and_run(void* urpc_vaddr);
void urpc_slave_init_and_run(void);
//does not delete the string. Todo: version which does?
void sendstring(char* str);
void sendram(struct capref* cap);
void urpc_spawn_process(char* name);
bool urpc_ram_is_initalized(void);