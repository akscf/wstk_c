/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_SERVLET_WEBSOCK_H
#define WSTK_SERVLET_WEBSOCK_H
#include <wstk-core.h>
#include <wstk-httpd.h>
#include <wstk-websock.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_servlet_websock_s wstk_servlet_websock_t;
typedef struct {
    wstk_http_conn_t        *http_conn;
    wstk_httpd_sec_ctx_t    *sec_ctx;
    wstk_websock_hdr_t      *header;
} wstk_servlet_websock_conn_t;

typedef void (*wstk_servlet_websock_on_close_t)(wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *ctx);
typedef bool (*wstk_servlet_websock_on_accept_t)(wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *ctx);
typedef void (*wstk_servlet_websock_on_message_t)(wstk_servlet_websock_conn_t *conn, wstk_mbuf_t *mbuf);

wstk_status_t wstk_httpd_register_servlet_websock(wstk_httpd_t *srv, char *path, wstk_servlet_websock_t **servlet);

wstk_status_t wstk_servlet_websock_set_on_close(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_close_t handler);
wstk_status_t wstk_servlet_websock_set_on_accept(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_accept_t handler);
wstk_status_t wstk_servlet_websock_set_on_message(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_message_t handler);

wstk_status_t wstk_servlet_websock_send(wstk_http_conn_t *conn, websock_opcode_e opcode, const char *fmt, ...);
wstk_status_t wstk_servlet_websock_send2(wstk_servlet_websock_t *servlet, uint32_t conn_id, websock_opcode_e opcode, const char *fmt, ...);



#ifdef __cplusplus
}
#endif
#endif
