#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <aos/aos.h>

struct nameserver_properties {
    char* prop_name;
    char* prop_attr;
};

//todo: rich query language with comparators and such. Expect many types to be made. Or a union to be used.
//Haskell type decl for query language:
//short circuit for just "is name = foo" and "is type = foo" should probably remain for convenience in setting up a nsq
//while this below is a very powerful query language, it is a lot of work to implement, especially if one somehow wants to make it
//      user friendly in C, so for now it exists just schematically here
//data Query = QName (Ops1 OpsString) | QType (Ops1 OpsString) | QComplex Query2
//data Ops1 a = Normal a | Negated a
//data Ops2 = Any [Ops2] | All [Ops2] | Ops2Str OpsString
//data OpsString = Is String | BeginsWith String | EndsWith String | Contains String
//data Query2 = Q2 [(Ops1 Ops2,[Ops1 Ops2])]
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
extern void free_nameserver_info(struct nameserver_info* nsi);

#endif //NAMESERVER_H
