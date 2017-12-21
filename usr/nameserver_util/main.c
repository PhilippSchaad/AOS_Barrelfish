#include <aos/aos.h>
#include <nameserver.h>
#include <aos/nameserver_internal.h> //this is only needed for pretty printing

#include <stdio.h>
#include <stdlib.h>
#include <aos/aos_rpc_shared.h>

struct lmp_chan recv_chan;


int main(int argc, char *argv[])
{
    debug_printf("Nameserver Util reporting in!\n");
    for (int i = 1; i < argc; ++i) {
        if(0 == strcmp(argv[i],"remove_self")) {
            printf("removing self\n");
            CHECK(remove_self());
        } else if(0 == strcmp(argv[i],"register")) {
            printf("registering FlatHierarchy\n");
            struct nameserver_info nsi;
            nsi.name = "FlatHierarchy";
            nsi.type = "Nothing";
            nsi.nsp_count = 0;
            //this is bad
/*            struct capref cap;
            debug_printf("print cap: slot %u, level %u, cnode %u, croot %u\n",(unsigned int)cap.slot,(
                    unsigned int) cap.cnode.level, (
                                 unsigned int) cap.cnode.cnode, (unsigned int) cap.cnode.croot);
            errval_t err = (slot_alloc(&cap));
            debug_printf("print cap: slot %u, level %u, cnode %u, croot %u\n",(unsigned int)cap.slot,(
                    unsigned int) cap.cnode.level, (
                                 unsigned int) cap.cnode.cnode, (unsigned int) cap.cnode.croot);
            nsi.chan_cap = cap;
            debug_printf("print cap: slot %u, level %u, cnode %u, croot %u\n",(unsigned int)nsi.chan_cap.slot,(
            unsigned int) nsi.chan_cap.cnode.level, (
            unsigned int) nsi.chan_cap.cnode.cnode, (unsigned int) nsi.chan_cap.cnode.croot);
            if(err != SYS_ERR_OK)
                debug_printf("had an error!\n"); */
            nsi.coreid = disp_get_core_id();
            init_rpc_server(NULL,&recv_chan);
            nsi.chan_cap = recv_chan.local_cap;
            CHECK(register_service(&nsi));
        } else if(0 == strcmp(argv[i],"deregister")) {
            printf("deregistering FlatHierarchy\n");
            CHECK(deregister("FlatHierarchy"));
        } else if(0 == strcmp(argv[i],"lookup")) {
            printf("looking up FlatHierarchy\n");
            struct nameserver_query nsq;
            nsq.tag = nsq_name;
            nsq.name = "FlatHierarchy";
            struct nameserver_info *nsi;
            CHECK(lookup(&nsq,&nsi));
            if(nsi != NULL) {
                char *ser = serialize_nameserver_info(nsi);
                ser += sizeof(unsigned int) + sizeof(int);
                printf("NSI with core id %u, and propcount %d: %s\n", nsi->coreid, nsi->nsp_count, ser);
            }else{
                printf("Could not find process with query: %s\n",nsq.name);
            }
        } else if(0 == strcmp(argv[i],"enumerate")) {
            struct nameserver_query nsq;
            nsq.tag = nsq_name;
            nsq.name = "FlatHierarchy";
            char** res;
            size_t num;
            CHECK(enumerate(&nsq,&num,&res));
            if(res != NULL) {
                debug_printf("enumerate found: \n");
                for(size_t j = 0; j < num; j++) {
                    debug_printf("    %s\n",res[j]);
                }
                debug_printf("end of enumeration\n");
            }else{
                printf("Could not find any processes with query: %s\n",nsq.name);
            }

        } else if(0 == strcmp(argv[i],"enumerate_complex")) {
        } else if(0 == strcmp(argv[i],"dump")) {
            ns_debug_dump();
        } else {
            printf("unknown command, known are: remove_self, register, deregister, lookup, enumerate, enumerate_complex\n");
        }
    }
}