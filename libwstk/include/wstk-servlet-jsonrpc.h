/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_SERVLET_JSONRPC_H
#define WSTK_SERVLET_JSONRPC_H
#include <wstk-core.h>
#include <wstk-httpd.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RPC_ORIGIN_SERVER           = 1,
    RPC_ORIGIN_METHOD           = 2,
    RPC_ORIGIN_TRANSPORT        = 3,
    RPC_ORIGIN_CLIENT           = 4,
    //
    RPC_ERROR_ILLEGAL_SERVICE   = 1,
    RPC_ERROR_SERVICE_NOT_FOUND = 2,
    RPC_ERROR_CLASS_NOT_FOUND   = 3,
    RPC_ERROR_METHOD_NOT_FOUND  = 4,
    RPC_ERROR_PARAMETR_MISMATCH = 5,
    RPC_ERROR_PERMISSION_DENIED = 6
};

typedef struct wstk_servlet_jsonrpc_s wstk_servlet_jsonrpc_t;
typedef struct wstk_servlet_jsonrpc_handler_result_s wstk_servlet_jsonrpc_handler_result_t;

typedef wstk_servlet_jsonrpc_handler_result_t *(*wstk_servlet_jsonrpc_service_handler_t)(wstk_httpd_sec_ctx_t *ctx, const char *method, const cJSON *params);

wstk_status_t wstk_httpd_register_servlet_jsonrpc(wstk_httpd_t *srv, char *path, wstk_servlet_jsonrpc_t **servlet);

wstk_status_t wstk_servlet_jsonrpc_register_service(wstk_servlet_jsonrpc_t *servlet, char *name, wstk_servlet_jsonrpc_service_handler_t handler, void *udata, bool auto_destroy);
wstk_status_t wstk_servlet_jsonrpc_unregister_service(wstk_servlet_jsonrpc_t *servlet, char *name);

#define wstk_jsonrpc_ok  wstk_servlet_jsonrpc_handler_result_ok
#define wstk_jsonrpc_err wstk_servlet_jsonrpc_handler_result_error
wstk_servlet_jsonrpc_handler_result_t *wstk_servlet_jsonrpc_handler_result_ok(cJSON *data);
wstk_servlet_jsonrpc_handler_result_t *wstk_servlet_jsonrpc_handler_result_error(uint32_t code, const char *message);



#ifdef __cplusplus
}
#endif
#endif
