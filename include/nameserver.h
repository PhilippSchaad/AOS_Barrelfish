#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <aos/aos.h>

struct nameserver_properties {
    char* prop_name;
    char* prop_attr;
};

//todo: rich query language with comparators and such. Expect many types to be made. Or a union to be used.

enum nameserver_query_tag { nsq_name, nsq_type};
struct nameserver_query {
   enum nameserver_query_tag tag;
   union {
       char *name;
       char *type;
   };
};

struct nameserver_info {
    char* name;
    char* type;
    unsigned int coreid;
    int nsp_count;
    struct nameserver_properties* props;
    struct capref chan_cap;
};

extern errval_t remove_self(void); //convenience function which removes all services this process added
extern errval_t register_service(struct nameserver_info *nsi);
extern errval_t deregister(char* name);
extern errval_t lookup(struct nameserver_query* nsq, struct nameserver_info** result); //first fit
extern errval_t enumerate(struct nameserver_query* nsq, size_t *num, char*** result); //all hits, names only
extern errval_t enumerate_complex(struct nameserver_query* nsq, size_t *num, struct nameserver_info** result); //all hits


#endif //NAMESERVER_H
