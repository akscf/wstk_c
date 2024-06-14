/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_TCP_SRV_H
#define WSTK_TCP_SRV_H
#include <wstk-core.h>
#include <wstk-net.h>
#include <wstk-poll.h>
#include <wstk-mutex.h>
#include <wstk-mbuf.h>
#include <wstk-hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_tcp_srv_s wstk_tcp_srv_t;
typedef struct wstk_tcp_srv_conn_s wstk_tcp_srv_conn_t;

typedef void (*wstk_tcp_srv_handler_t)(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf);

wstk_status_t wstk_tcp_srv_create(wstk_tcp_srv_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t max_idle, uint32_t buffer_size, wstk_tcp_srv_handler_t handler);
wstk_status_t wstk_tcp_ssl_srv_create(wstk_tcp_srv_t **srv, wstk_sockaddr_t *address, char *cert, uint32_t max_conns, uint32_t max_idle, uint32_t buffer_size, wstk_tcp_srv_handler_t handler);
wstk_status_t wstk_tcp_srv_start(wstk_tcp_srv_t *srv);

wstk_status_t wstk_tcp_srv_id(wstk_tcp_srv_t *srv, uint32_t *id);
wstk_status_t wstk_tcp_srv_polling_method(wstk_tcp_srv_t *srv, wstk_polling_method_e *method);
wstk_status_t wstk_tcp_srv_listen_address(wstk_tcp_srv_t *srv, wstk_sockaddr_t **laddr);

bool wstk_tcp_srv_is_ready(wstk_tcp_srv_t *srv);
bool wstk_tcp_srv_is_destroyed(wstk_tcp_srv_t *srv);

wstk_tcp_srv_conn_t *wstk_tcp_srv_lookup_conn(wstk_tcp_srv_t *srv, uint32_t id);
wstk_status_t wstk_tcp_srv_conn_take(wstk_tcp_srv_conn_t *conn);
wstk_status_t wstk_tcp_srv_conn_release(wstk_tcp_srv_conn_t *conn);

wstk_status_t wstk_tcp_srv_conn_close(wstk_tcp_srv_conn_t *conn);
bool wstk_tcp_srv_conn_is_destroyed(wstk_tcp_srv_conn_t *conn);
bool wstk_tcp_srv_conn_is_closed(wstk_tcp_srv_conn_t *conn);

wstk_status_t wstk_tcp_srv_conn_id(wstk_tcp_srv_conn_t *conn, uint32_t *id);
wstk_status_t wstk_tcp_srv_conn_peer(wstk_tcp_srv_conn_t *conn, wstk_sockaddr_t **peer);
wstk_status_t wstk_tcp_srv_conn_socket(wstk_tcp_srv_conn_t *conn, wstk_socket_t **sock);
wstk_status_t wstk_tcp_srv_conn_server(wstk_tcp_srv_conn_t *conn, wstk_tcp_srv_t **srv);

wstk_status_t wstk_tcp_srv_conn_write(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout);
wstk_status_t wstk_tcp_srv_conn_read(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout);

wstk_status_t wstk_tcp_srv_conn_rdlock(wstk_tcp_srv_conn_t *conn, bool flag);

wstk_status_t wstk_tcp_srv_attr_add(wstk_tcp_srv_t *srv, const char *name, void *value, bool auto_destroy);
wstk_status_t wstk_tcp_srv_attr_get(wstk_tcp_srv_t *srv, const char *name, void **value);
wstk_status_t wstk_tcp_srv_attr_del(wstk_tcp_srv_t *srv, const char *name);

wstk_status_t wstk_tcp_srv_conn_attr_add(wstk_tcp_srv_conn_t *conn, const char *name, void *value, bool auto_destroy);
wstk_status_t wstk_tcp_srv_conn_attr_get(wstk_tcp_srv_conn_t *conn, const char *name, void **value);
wstk_status_t wstk_tcp_srv_conn_attr_del(wstk_tcp_srv_conn_t *conn, const char *name);



#ifdef __cplusplus
}
#endif
#endif
