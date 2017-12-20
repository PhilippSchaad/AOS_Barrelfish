
#include <aos/aos.h>
#include <nameserver.h>
#include <aos/nameserver_internal.h>

#include <stdio.h>
#include <stdlib.h>


char* serialize_nameserver_prop(struct nameserver_properties* ns_p) {
    assert(ns_p != NULL);
    assert(ns_p->prop_name != NULL);
    assert(ns_p->prop_attr != NULL);
    size_t len1 = strlen(ns_p->prop_name);
    size_t len = len1;
    len+=strlen(ns_p->prop_attr);
    len+=4;
    char* res = malloc(len);
    res[0] = '{';
    strcpy(&res[1],ns_p->prop_name);
    res[1+len1] = ':';
    strcpy(&res[2+len1],ns_p->prop_attr);
    res[len-1] = '}';
    res[len] = '\0';
    return res;
}
errval_t deserialize_nameserver_prop(char* input, struct nameserver_properties** ns_p_out, int* str_consumed) {
    debug_printf("des_ns_prop input: '%s'\n",input);
    char* org_input = input;
    *ns_p_out = malloc(sizeof(struct nameserver_properties));
    if(*input != '{') {
        assert(!"encoding error");
        return -1; //todo: real error
    }
    input++;
    char* input2 = input;
    size_t len = 0;
    while(*input2 != ':') {
        if(*input2 == '}' || *input2 == '{') {
            assert(!"another encoding error");
            return -1; //todo: real error
        }
        len++;
        input2++;
    }
    debug_printf("prop_name len: %u\n",len);
    (*ns_p_out)->prop_name = malloc(len+1);
    strncpy((*ns_p_out)->prop_name,input,len);
    (*ns_p_out)->prop_name[len] = '\0';
    input += len+1;
    input2 = input;
    len = 0;
    while(*input2 != '}') {
        len++;
        input2++;
    }
    debug_printf("prop_attr len: %u\n",len);
    (*ns_p_out)->prop_attr = malloc(len+1);
    strncpy((*ns_p_out)->prop_attr,input,len);
    (*ns_p_out)->prop_attr[len] = '\0';
    *str_consumed = input2+1-org_input;
    return SYS_ERR_OK;
}

//todo: fix hacks when implementing proper queries
char* serialize_nameserver_query(struct nameserver_query* ns_q) {
    size_t len = 0;
    switch(ns_q->tag) {
        case nsq_name:
            len = strlen(ns_q->name);
            break;
        case nsq_type:
            len = strlen(ns_q->type);
            break;
    }
    char * res = malloc(len+1+sizeof(enum nameserver_query_tag));
    char *res2 = res;
    memcpy(res,&(ns_q->tag),sizeof(enum nameserver_query_tag));
    res2 += sizeof(enum nameserver_query_tag);
    switch(ns_q->tag) {
        case nsq_name:
            strncpy(res2,ns_q->name,len);
            break;
        case nsq_type:
            strncpy(res2,ns_q->type,len);
            break;
    }
    res2[len] = '\0';
    debug_printf(res2);
    debug_printf("%u\n",(unsigned int) sizeof(enum nameserver_query_tag));
    return res;
}
errval_t deserialize_nameserver_query(char* input, struct nameserver_query** ns_q_out) {
    *ns_q_out = malloc(sizeof(struct nameserver_query));
    (*ns_q_out)->tag = *(enum nameserver_query_tag*)input;
    input += sizeof(enum nameserver_query_tag);
    size_t len = strlen(input);
    char *str = malloc(len+1);
    strcpy(str,input);
    str[len] = '\0';
    debug_printf(str);
    switch((*ns_q_out)->tag) {
        case nsq_name:
            (*ns_q_out)->name = str;
            break;
        case nsq_type:
            (*ns_q_out)->type = str;
            break;
    }
    return SYS_ERR_OK;
}

char* serialize_nameserver_info(struct nameserver_info* ns_i) {
    //no comma needed between props as they are serialized as {name:attr}
    //coreid and nsp_count are static integers of known length so we just put them at the front
    //CoreidNsp_count{name,type,{PropPropProp...}}
    size_t namelen = strlen(ns_i->name);
    size_t typelen = strlen(ns_i->type);
    size_t proplens = 0;
    char** serialized_props = malloc(sizeof(char*)*ns_i->nsp_count);
    for(int i = 0; i < ns_i->nsp_count; i++) {
        serialized_props[i] = serialize_nameserver_prop(&ns_i->props[i]);
        proplens += strlen(serialized_props[i]);
    }
    size_t otherlen = sizeof(unsigned int)+sizeof(int)+1+1+2+2+1;
    char *res = malloc(namelen+typelen+proplens+otherlen+1);
    char *out = res;
    memcpy(res,&ns_i->coreid,sizeof(unsigned int)); res += sizeof(unsigned int);
    memcpy(res,&ns_i->nsp_count,sizeof(int)); res += sizeof(int);
    *res = '{'; res++;
    strcpy(res,ns_i->name); res += namelen;
    *res = ','; res++;
    strcpy(res,ns_i->type); res += typelen;
    *res = ','; res++;
    *res = '{'; res++;
    for(int i = 0; i < ns_i->nsp_count; i++) {
        strcpy(res,serialized_props[i]); res += strlen(serialized_props[i]);
    }
    *res = '}'; res++;
    *res = '}'; res++;
    res = '\0';
    return out;
}
errval_t deserialize_nameserver_info(char* input, struct nameserver_info** ns_i_out) {
    struct nameserver_info* out = malloc(sizeof(struct nameserver_info));

    memcpy(&out->coreid,input, sizeof(unsigned int)); input += sizeof(unsigned int);
    memcpy(&out->nsp_count,input, sizeof(int)); input += sizeof(int);
    input += 1; //todo maybe that it is '{'
    char* dis = strchr(input,',');
    if(dis == NULL) {
        debug_printf("%p, %s\n",input,input);
        assert(!"encoding_error");
    }
    size_t len = dis-input;
    if(len > 10000) {
        debug_printf("%p-%p",dis,input);
        assert(!"this ain't right\n");
    }
    out->name = malloc(len+1);
    strncpy(out->name,input,len);
    out->name[len] = '\0';
    debug_printf("des_ns_info name: '%s'\n",out->name);
    input += len+1;

    dis = strchr(input,',');
    len = dis-input;
    out->type = malloc(len+1);
    strncpy(out->type,input,len);
    out->type[len] = '\0';
    debug_printf("des_ns_info name: '%s'\n",out->type);
    input += len+1;
    debug_printf("des_ns_info remstr: '%s'\n",input);
    input++; //todo: maybe check that it is '{'
    out->props = malloc(out->nsp_count * sizeof(struct nameserver_properties));
    for(int i = 0; i < out->nsp_count; i++) {
        struct nameserver_properties* prop;
        int str_consumed;
        deserialize_nameserver_prop(input,&prop,&str_consumed);
        out->props[i] = *prop;
        debug_printf("printing prop: {%s:%s}\n",prop->prop_name,prop->prop_attr);
        input+=str_consumed;
        free(prop);
    }
    *ns_i_out = out;
    return SYS_ERR_OK;
}

