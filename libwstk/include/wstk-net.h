/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_NET_H
#define WSTK_NET_H
#include <wstk-core.h>
#include <wstk-ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_RW_ACCEPTABLE(st) (st == WSTK_STATUS_SUCCESS || st == WSTK_STATUS_NODATA)
#define WSTK_RD_TIMEOUT(l) (10)
#define WSTK_WR_TIMEOUT(l) (l > 102400 ? 10 : 0)

#ifdef WSTK_OS_WIN
 #define WSTK_SOCK_ERROR WSAGetLastError()
#else
 #define WSTK_SOCK_ERROR errno
#endif

/* net-util */
int wstk_net_ntop(int af, const void *src, char *dst, size_t size);
int wstk_net_pton(int af, const char *src, void *dst);


/* socket-address */
typedef struct wstk_sockaddr_s {
    union {
        struct sockaddr sa;
        struct sockaddr_in in;
#ifdef WSTK_NET_HAVE_INET6
        struct sockaddr_in6 in6;
#endif
        uint8_t padding[28];
    } u;
    socklen_t   len;
    int         err;        // copy of errno
} wstk_sockaddr_t;

typedef enum {
    WSTK_SA_ADDR = (1<<0),
    WSTK_SA_PORT = (1<<1),
    WSTK_SA_ALL  = (WSTK_SA_ADDR | WSTK_SA_PORT)
} wstk_sa_flag_e;

wstk_status_t wstk_sa_inet_pton(const char *addr, wstk_sockaddr_t *sa);
wstk_status_t wstk_sa_inet_ntop(const wstk_sockaddr_t *sa, char *buf, int size);

wstk_status_t wstk_sa_init(wstk_sockaddr_t *sa, int af);
wstk_status_t wstk_sa_decode(wstk_sockaddr_t *sa, const char *str, size_t len);

wstk_status_t wstk_sa_set_pl(wstk_sockaddr_t *sa, const wstk_pl_t *addr, uint16_t port);
wstk_status_t wstk_sa_set_str(wstk_sockaddr_t *sa, const char *addr, uint16_t port);
wstk_status_t wstk_sa_set_in(wstk_sockaddr_t *sa, uint32_t addr, uint16_t port);
wstk_status_t wstk_sa_set_in6(wstk_sockaddr_t *sa, const uint8_t *addr, uint16_t port);
wstk_status_t wstk_sa_set_sa(wstk_sockaddr_t *sa, const struct sockaddr *s);
wstk_status_t wstk_sa_set_port(wstk_sockaddr_t *sa, uint16_t port);

wstk_status_t wstk_sa_af(const wstk_sockaddr_t *sa, int *af);
wstk_status_t wstk_sa_in(const wstk_sockaddr_t *sa, uint32_t *in);
wstk_status_t wstk_sa_in6(const wstk_sockaddr_t *sa, uint8_t *addr);
wstk_status_t wstk_sa_port(const wstk_sockaddr_t *sa, uint16_t *port);

bool wstk_sa_is_set(const wstk_sockaddr_t *sa, int flag);
bool wstk_sa_is_any(const wstk_sockaddr_t *sa);
bool wstk_sa_is_loopback(const wstk_sockaddr_t *sa);
bool wstk_sa_is_linklocal(const wstk_sockaddr_t *sa);

bool wstk_sa_cmp(const wstk_sockaddr_t *l, const wstk_sockaddr_t *r, int flag);
wstk_status_t wstk_sa_cpy(wstk_sockaddr_t *dst, const wstk_sockaddr_t *src);
wstk_status_t wstk_sa_hash(const wstk_sockaddr_t *sa, uint32_t *hash);

/* socket */
typedef struct {
    wstk_ssl_ctx_t  *ssl_ctx;           //
    void            *udata;             // user data
    int             type;               // AF_INET or AF_INET6
    int             proto;              // UDP / TCP
    int             err;                // errno
    int             fd;                 // socket fd
    uint32_t        pmask;              // poll mask (wstk_poll_mask_e)
    uint32_t        rderr;              // helper to detect tcp eof without poll
    time_t          expiry;             // idle timeout
    bool            fl_connected;       // uses by client
    bool            fl_destroyed;       // destroyed but has refs
    bool            fl_adestroy_udata;  // destroy udata when socket closing
} wstk_socket_t;

wstk_status_t wstk_sock_alloc(wstk_socket_t **sock, int type, int proto, int fd);
wstk_status_t wstk_sock_get_local(wstk_socket_t *sock, wstk_sockaddr_t *local);
wstk_status_t wstk_sock_get_peer(wstk_socket_t *sock, wstk_sockaddr_t *peer);
wstk_status_t wstk_sock_get_bytes_to_read(wstk_socket_t *sock, size_t *bytes);
wstk_status_t wstk_sock_close_fd(wstk_socket_t *sock);

wstk_status_t wstk_sock_set_udata(wstk_socket_t *sock, void *udata, bool auto_destroy);
wstk_status_t wstk_sock_set_pmask(wstk_socket_t *sock, uint32_t mask);
wstk_status_t wstk_sock_set_expiry(wstk_socket_t *sock, uint32_t timeout);

wstk_status_t wstk_sock_set_blocking(wstk_socket_t *sock, bool blocking);
wstk_status_t wstk_sock_set_reuse(wstk_socket_t *sock, bool reuse);
wstk_status_t wstk_sock_set_nodelay(wstk_socket_t *sock, bool val);

/* UDP */
wstk_status_t wstk_udp_connect(wstk_socket_t **sock, const wstk_sockaddr_t *peer);
wstk_status_t wstk_udp_listen(wstk_socket_t **sock, const wstk_sockaddr_t *local);

wstk_status_t wstk_udp_send(wstk_socket_t *sock, const wstk_sockaddr_t *dst, wstk_mbuf_t *mbuf);
wstk_status_t wstk_udp_recv(wstk_socket_t *sock, wstk_sockaddr_t *src, wstk_mbuf_t *mbuf, uint32_t timeout);

wstk_status_t wstk_udp_multicast_join(wstk_socket_t *sock, const wstk_sockaddr_t *group);
wstk_status_t wstk_udp_multicast_leave(wstk_socket_t *sock, const wstk_sockaddr_t *group);

/* TCP */
wstk_status_t wstk_tcp_connect(wstk_socket_t **sock, const wstk_sockaddr_t *peer, uint32_t timeout);
wstk_status_t wstk_tcp_listen(wstk_socket_t **sock, const wstk_sockaddr_t *local, uint32_t nconn);

wstk_status_t wstk_tcp_accept(wstk_socket_t *sock, wstk_socket_t **soc_new, uint32_t timeout);

wstk_status_t wstk_tcp_write(wstk_socket_t *sock, wstk_mbuf_t *mbuf, uint32_t timeout);
wstk_status_t wstk_tcp_read(wstk_socket_t *sock, wstk_mbuf_t *mbuf, uint32_t timeout);

wstk_status_t wstk_tcp_printf(wstk_socket_t *sock, const char *fmt, ...);
wstk_status_t wstk_tcp_vprintf(wstk_socket_t *sock, const char *fmt, va_list ap);



#ifdef __cplusplus
}
#endif
#endif

