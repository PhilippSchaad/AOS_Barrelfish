#include <aos/aos.h>
#include <nameserver.h>
#include <aos/nameserver_internal.h> //this is only needed for pretty printing

#include <stdio.h>
#include <stdlib.h>
#include <aos/aos_rpc_shared.h>

struct lmp_chan recv_chan;

static void print_enumeration(size_t num, char **res) {
    printf("enumerate found %u results: \n",num);
    for(size_t j = 0; j < num; j++) {
        printf("    %s\n",res[j]);
    }
    printf("end of enumeration\n");
}

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
                free_nameserver_info(nsi);
            }else{
                printf("Could not find process with query: %s\n",nsq.name);
            }
        } else if(0 == strcmp(argv[i],"enumerate")) {
            struct nameserver_query nsq;
            struct query_prop qp;
            struct nameserver_properties qpp;
            if(i+2 < argc) {
                if(argv[i+1][0] == '0') {
                    printf("querying for name %s\n",argv[i+2]);
                    nsq.tag = nsq_name;
                    nsq.name = argv[i+2];
                    i+=2;
                }
                else if(argv[i+1][0] == '1') {
                    printf("querying for type %s\n",argv[i+2]);
                    nsq.tag = nsq_type;
                    nsq.type = argv[i+2];
                    i+=2;
                }
                else if(argv[i+1][0] == '2') {
                    printf("querying for props...\n");
                    nsq.tag = nsq_props;
                    nsq.qp.count = 1;
                    bool invalid = false;
                    switch (argv[i + 1][1]) {
                        case '0':
                            printf("querying for equality\n");
                            qp.nsqpt = nsqp_is;
                            break;
                        case '1':
                            printf("querying for same beginning\n");
                            qp.nsqpt = nsq_beginswith;
                            break;
                        case '2':
                            printf("querying for same ending\n");
                            qp.nsqpt = nsq_endswith;
                            break;
                        case '3':
                            printf("querying for containing\n");
                            qp.nsqpt = nsq_contains;
                            break;
                        default:
                            printf("invalid test for nsq_props query, needs to be from 0 to 3\n");
                            invalid = true;
                            break;
                    }
                    if (invalid)
                        continue;
                    printf("the prop to query for is: %s\n", argv[i + 2]);
                    char *t = argv[i + 2];
                    invalid = true;
                    while (*t) {
                        if (*t == ':') {
                            *t = '\0';
                            t++;
                            invalid = false;
                            break;
                        }
                        t++;
                    }
                    if (invalid)
                        continue;
                    qpp.prop_name = argv[i + 2];
                    qpp.prop_attr = t;
                    qp.nsp = &qpp;
                    nsq.qp.qps = &qp;
                    i+=2;
                }else if(argv[i+1][0] == '3') {
                    printf("enumerating for name FlatHierarchy\n");
                    nsq.tag = nsq_name;
                    nsq.name = "FlatHierarchy";
                    i++;
                }else{
                    printf("invalid range for query, needs to be from 0 to 3\n");
                    continue;
                }
            }else{
                nsq.tag = nsq_name;
                nsq.name = "FlatHierarchy";
            }
            char** res;
            size_t num;
            CHECK(enumerate(&nsq,&num,&res));
            debug_printf("test\n");
            if(res != NULL) {
                debug_printf("here\n");
                print_enumeration(num, res);
            }else
                printf("Could not find any processes with query\n");
        } else if(0 == strcmp(argv[i],"enumerate_all")) {
            struct nameserver_query nsq;
            nsq.tag = nsq_props;
            nsq.qp.count = 0;
            nsq.qp.qps = NULL;
            char** res;
            size_t num;
            CHECK(enumerate(&nsq,&num,&res));
            if(res != NULL) {
                print_enumeration(num, res);
            }else
                printf("Could not find any processes with query: %s\n",nsq.name);
        } else if(0 == strcmp(argv[i],"enumerate_all_complex")) {
            struct nameserver_query nsq;
            nsq.tag = nsq_props;
            nsq.qp.count = 0;
            nsq.qp.qps = NULL;
            char** res;
            size_t num;
            CHECK(enumerate(&nsq,&num,&res));
            if(res != NULL) {
                printf("enumerate found: \n");
                for(size_t j = 0; j < num; j++) {
                    struct nameserver_query nsq2;
                    nsq2.tag = nsq_name;
                    nsq2.name = res[j];
                    struct nameserver_info *nsi;
                    CHECK(lookup(&nsq2,&nsi));
                    if(nsi != NULL) {
                        char *ser = serialize_nameserver_info(nsi);
                        ser += sizeof(unsigned int) + sizeof(int);
                        printf("NSI with core id %u, and propcount %d: %s\n", nsi->coreid, nsi->nsp_count, ser);
                        free(ser-(sizeof(unsigned int) + sizeof(int)));
                        free_nameserver_info(nsi);
                    } else {
                        printf("got %s back but could not look that service up again\n",res[j]);
                    }
                }
                printf("end of enumeration\n");
            }else{
                printf("Could not find any processes with query: %s\n",nsq.name);
            }
        } else if(0 == strcmp(argv[i],"dump")) {
            ns_debug_dump();
        } else {
            printf("unknown command, known are: remove_self, register, deregister, lookup, enumerate, enumerate_all, enumerate_all_complex\n");
        }
    }
}