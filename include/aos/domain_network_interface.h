#ifndef _USR_NETWORK_DOMAIN_INTERFACE_H_
#define _USR_NETWORK_DOMAIN_INTERFACE_H_

#include <aos/aos.h>
#include <stdlib.h>

errval_t network_register_port(uint16_t port, uint16_t protocol, uint32_t pid, uint32_t core,  errval_t (*callback)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port));

#endif
