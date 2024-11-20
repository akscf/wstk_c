/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_UDP_SRV_H
#define WSTK_UDP_SRV_H
#include <wstk-core.h>
#include <wstk-net.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_udp_srv_s wstk_udp_srv_t;
typedef struct wstk_udp_srv_conn_s wstk_udp_srv_conn_t;

typedef void (*wstk_udp_srv_handler_t)(wstk_udp_srv_conn_t *conn, wstk_mbuf_t *message);

wstk_status_t wstk_udp_srv_create(wstk_udp_srv_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t buffer_size, wstk_udp_srv_handler_t handler);
wstk_status_t wstk_udp_srv_start(wstk_udp_srv_t *srv);

wstk_status_t wstk_udp_srv_id(wstk_udp_srv_t *srv, uint32_t *id);
wstk_status_t wstk_udp_srv_listen_address(wstk_udp_srv_t *srv, wstk_sockaddr_t **laddr);
bool wstk_udp_srv_is_ready(wstk_udp_srv_t *srv);
bool wstk_udp_srv_is_destroyed(wstk_udp_srv_t *srv);

wstk_status_t wstk_udp_srv_conn_id(wstk_udp_srv_conn_t *conn, uint32_t *id);
wstk_status_t wstk_udp_srv_conn_peer(wstk_udp_srv_conn_t *conn, wstk_sockaddr_t **peer);
wstk_status_t wstk_udp_srv_conn_udata(wstk_udp_srv_conn_t *conn, void **udata);
wstk_status_t wstk_udp_srv_conn_set_udata(wstk_udp_srv_conn_t *conn, void *udata, bool auto_destroy);

wstk_status_t wstk_udp_srv_write(wstk_udp_srv_conn_t *conn, wstk_mbuf_t *mbuf);

wstk_status_t wstk_udp_srv_attr_add(wstk_udp_srv_t *srv, const char *name, void *value, bool auto_destroy);
wstk_status_t wstk_udp_srv_attr_get(wstk_udp_srv_t *srv, const char *name, void **value);
wstk_status_t wstk_udp_srv_attr_del(wstk_udp_srv_t *srv, const char *name);



#ifdef __cplusplus
}
#endif
#endif
