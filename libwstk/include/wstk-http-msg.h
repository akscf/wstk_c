/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_HTTP_MSG_H
#define WSTK_HTTP_MSG_H
#include <wstk-core.h>
#include <wstk-mbuf.h>
#include <wstk-hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wstk_hash_t     *headers;           // headers
    wstk_pl_t       scheme;             // http / https
    wstk_pl_t       version;            // http version
    wstk_pl_t       method;             // regusest method
    wstk_pl_t       path;               // request path
    wstk_pl_t       params;             // request params
    wstk_pl_t       ctype;              // content type
    wstk_pl_t       charset;            // content charset
    uint32_t        clen;               // content lenght
} wstk_http_msg_t;

wstk_status_t wstk_http_msg_alloc(wstk_http_msg_t **msg);
wstk_status_t wstk_http_msg_decode(wstk_http_msg_t *msg, wstk_mbuf_t *buf);
wstk_status_t wstk_http_msg_dump(wstk_http_msg_t *msg);

wstk_status_t wstk_http_msg_header_add(wstk_http_msg_t *msg, const char *name, const char *value);
wstk_status_t wstk_http_msg_header_get(wstk_http_msg_t *msg, const char *name, const char **value);
wstk_status_t wstk_http_msg_header_del(wstk_http_msg_t *msg, const char *name);

bool wstk_http_msg_header_exists(wstk_http_msg_t *msg, const char *name);



#ifdef __cplusplus
}
#endif
#endif
