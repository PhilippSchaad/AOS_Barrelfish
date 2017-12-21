
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
    res[len-2] = '}';
    res[len-1] = '\0';
    DBG(VERBOSE,"serizalized prop: '%s'\n",res);
    return res;
}
errval_t deserialize_nameserver_prop(char* input, struct nameserver_properties** ns_p_out, int* str_consumed) {
    DBG(VERBOSE,"des_ns_prop input: '%s'\n",input);
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
    DBG(VERBOSE,"prop_name len: %u\n",len);
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
    DBG(VERBOSE,"prop_attr len: %u\n",len);
    (*ns_p_out)->prop_attr = malloc(len+1);
    strncpy((*ns_p_out)->prop_attr,input,len);
    (*ns_p_out)->prop_attr[len] = '\0';
    *str_consumed = input2+1-org_input;
    return SYS_ERR_OK;
}

char* serialize_nameserver_query(struct nameserver_query* ns_q, size_t *serlen) {
    size_t len = 0;
    char *res = NULL;
    if(ns_q->tag == nsq_name || ns_q->tag == nsq_type) {
        switch (ns_q->tag) {
            case nsq_name:
                len = strlen(ns_q->name);
                break;
            case nsq_type:
                len = strlen(ns_q->type);
                break;
            default:
                assert(!"never");
        }
        *serlen = len + 1 + sizeof(enum nameserver_query_tag);
        res = malloc(*serlen);
        char *res2 = res;
        memcpy(res, &(ns_q->tag), sizeof(enum nameserver_query_tag));
        res2 += sizeof(enum nameserver_query_tag);
        switch (ns_q->tag) {
            case nsq_name:
                strncpy(res2, ns_q->name, len);
                break;
            case nsq_type:
                strncpy(res2, ns_q->type, len);
                break;
            default:
                assert(!"never");
        }
        res2[len] = '\0';
        DBG(VERBOSE,"query ser: '%s'\n",res2);
    }else {
        if(ns_q->tag == nsq_props) {
            char **proparr = NULL;
            if(ns_q->qp.count > 0) {
                proparr = malloc(sizeof(char *) * ns_q->qp.count);
                for (int j = 0; j < ns_q->qp.count; j++) {
                    char *ser_prop = serialize_nameserver_prop(ns_q->qp.qps[j].nsp);
                    proparr[j] = ser_prop;
                    len += strlen(ser_prop) + sizeof(enum nameserver_queryprops_type);
//                    debug_printf("query prop %s\n",proparr[j]);
                }
            }
            res = malloc(len + 1 + sizeof(enum nameserver_query_tag) + sizeof(size_t));
            char *res2 = res;
            memcpy(res, &(ns_q->tag), sizeof(enum nameserver_query_tag));
            res2 += sizeof(enum nameserver_query_tag);
            *serlen += sizeof(enum nameserver_query_tag);
            memcpy(res2, &(ns_q->qp.count),sizeof(size_t));
            res2 += sizeof(size_t);
            *serlen += sizeof(size_t);
            if(ns_q->qp.count > 0) {
                for (int j = 0; j < ns_q->qp.count; j++) {
                    memcpy(res2, &ns_q->qp.qps[j].nsqpt,sizeof(enum nameserver_queryprops_type));
                    res2 += sizeof(enum nameserver_queryprops_type);
                    *serlen += sizeof(enum nameserver_queryprops_type);
                    strcpy(res2,proparr[j]);
//                    debug_printf("query prop %s\n",res2);
                    size_t len3 = strlen(proparr[j]);
                    res2 += len3;
                    *serlen += len3;
                }
            }
            *res2 = '\0';
            (*serlen)++;
//            debug_printf("ser'd count: %u and total size: %u\n",*debugres,*serlen);
        }else{
            assert(!"also never");
        }
    }
//    debug_printf("%u\n",(unsigned int) sizeof(enum nameserver_query_tag));
    return res;
}
//nameserver_util enumerate 20 IsSystemService:True

static errval_t deserialize_nsq_props(char* input, struct nameserver_query* ns_q) {
    DBG(VERBOSE,"des_nsq_props input: '%s'\n",input+sizeof(size_t));
    size_t count;
    memcpy(&count,input,sizeof(size_t));
    //debug_printf("temp count: %u\n",*(unsigned int*)input);
    ns_q->qp.count = count;
    input += sizeof(size_t);
    //debug_printf("count: %u\n",count);
    if(count == 0) {
        ns_q->qp.qps = NULL;
        return SYS_ERR_OK;
    }
    ns_q->qp.qps = malloc(sizeof(struct query_prop)*count);
    for(int i = 0; i < count; i++) {
        enum nameserver_queryprops_type nsqpt;
        memcpy(&nsqpt,input,sizeof(enum nameserver_queryprops_type));
        input += sizeof(enum nameserver_queryprops_type);
        ns_q->qp.qps[i].nsqpt = nsqpt;
        int k;
        errval_t  err = deserialize_nameserver_prop(input,&ns_q->qp.qps[i].nsp,&k);
        input += k;
        if(!err_is_ok(err))
            return err;
    }
    return SYS_ERR_OK;
}

errval_t deserialize_nameserver_query(char* input, struct nameserver_query** ns_q_out) {
    *ns_q_out = malloc(sizeof(struct nameserver_query));
    (*ns_q_out)->tag = *(enum nameserver_query_tag*)input;
    input += sizeof(enum nameserver_query_tag);
    size_t len = strlen(input);
    char *str = malloc(len+1);
    strcpy(str,input);
    str[len] = '\0';
    DBG(VERBOSE,"query core string des: '%s'\n",str);
//    debug_printf("des'd prop tag: %u\n",(unsigned int)(*ns_q_out)->tag);
    switch((*ns_q_out)->tag) {
        case nsq_name:
            (*ns_q_out)->name = str;
            break;
        case nsq_type:
            (*ns_q_out)->type = str;
            break;
        case nsq_props:
            deserialize_nsq_props(input,*ns_q_out);
            free(str);
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
        DBG(VERBOSE,"prop ser: '%s'\n",serialized_props[i]);
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
    *res = '\0';
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
    DBG(VERBOSE,"des_ns_info name: '%s'\n",out->name);
    input += len+1;

    dis = strchr(input,',');
    len = dis-input;
    out->type = malloc(len+1);
    strncpy(out->type,input,len);
    out->type[len] = '\0';
    DBG(VERBOSE,"des_ns_info name: '%s'\n",out->type);
    input += len+1;
    DBG(VERBOSE,"des_ns_info remstr: '%s'\n",input);
    input++; //todo: maybe check that it is '{'
    out->props = malloc(out->nsp_count * sizeof(struct nameserver_properties));
    for(int i = 0; i < out->nsp_count; i++) {
        struct nameserver_properties* prop;
        int str_consumed;
        deserialize_nameserver_prop(input,&prop,&str_consumed);
        out->props[i] = *prop;
        DBG(VERBOSE,"printing prop: {%s:%s}\n",prop->prop_name,prop->prop_attr);
        input+=str_consumed;
        free(prop);
    }
    *ns_i_out = out;
    return SYS_ERR_OK;
}

void free_nameserver_info(struct nameserver_info* nsi) {
    free(nsi->name);
    free(nsi->type);
    for(int i = 0; i < nsi->nsp_count; i++) {
        free(nsi->props[i].prop_name);
        free(nsi->props[i].prop_attr);
    }
    if(nsi->nsp_count > 0)
        free(nsi->props);
    free(nsi);
}
