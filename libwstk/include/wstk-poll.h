/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_POLL_H
#define WSTK_POLL_H
#include <wstk-core.h>
#include <wstk-net.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WSTK_POLL_AUTO,
    WSTK_POLL_SELECT,
    WSTK_POLL_KQUEUE,
    WSTK_POLL_EPOLL
} wstk_polling_method_e;

typedef enum {
    WSTK_POLL_MREAD      = (1<<0),
    WSTK_POLL_MWRITE     = (1<<1),
    WSTK_POLL_MEXCEPT    = (1<<2),
    // special flags
    WSTK_POLL_MLISTENER  = (1<<3),
    WSTK_POLL_MRDLOCK    = (1<<4),
} wstk_poll_socket_mask_e;

typedef enum {
    WSTK_POLL_EREAD     = (1<<0),
    WSTK_POLL_EWRITE    = (1<<1),
    WSTK_POLL_EEXCEPT   = (1<<2),
    WSTK_POLL_ESCLOSED  = (1<<3),
    WSTK_POLL_ESEXPIRED = (1<<4)
} wstk_poll_socket_event_e;

typedef enum {
    WSTK_POLL_FYIELD_ON_EMPTY   = (1<<0) // yield or 1s pause
} wstk_poll_flags_e;

typedef void (*wstk_poll_handler_t)(wstk_socket_t *socket, int event, void *udata);

typedef struct wstk_poll_s wstk_poll_t;
bool wstk_poll_is_empty(wstk_poll_t *poll);
wstk_status_t wstk_poll_method(wstk_poll_t *poll, wstk_polling_method_e *method);
wstk_status_t wstk_poll_interrupt(wstk_poll_t *poll);
wstk_status_t wstk_poll_size(wstk_poll_t *poll, uint32_t *size);
wstk_status_t wstk_poll_space(wstk_poll_t *poll, uint32_t *space);
wstk_status_t wstk_poll_create(wstk_poll_t **poll, wstk_polling_method_e method, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata);
wstk_status_t wstk_poll_add(wstk_poll_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_del(wstk_poll_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_polling(wstk_poll_t *poll);

typedef struct {
    wstk_polling_method_e   method;
    uint32_t                size;
} wstk_poll_auto_conf_t;
wstk_poll_auto_conf_t wstk_poll_auto_conf(wstk_polling_method_e method, uint32_t size);

/* select */
typedef struct wstk_poll_select_s wstk_poll_select_t;
bool wstk_poll_select_is_supported();
bool wstk_poll_select_is_empty(wstk_poll_select_t *poll);
uint32_t wstk_poll_select_max_size();
wstk_status_t wstk_poll_select_size(wstk_poll_select_t *poll, uint32_t *size);
wstk_status_t wstk_poll_select_space(wstk_poll_select_t *poll, uint32_t *space);
wstk_status_t wstk_poll_select_create(wstk_poll_select_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata);
wstk_status_t wstk_poll_select_add(wstk_poll_select_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_select_del(wstk_poll_select_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_select_polling(wstk_poll_select_t *poll);


/* kqueue */
typedef struct wstk_poll_kqueue_s wstk_poll_kqueue_t;
bool wstk_poll_kqueue_is_supported();
bool wstk_poll_kqueue_is_empty(wstk_poll_kqueue_t *poll);
wstk_status_t wstk_poll_kqueue_interrupt(wstk_poll_kqueue_t *poll);
wstk_status_t wstk_poll_kqueue_size(wstk_poll_kqueue_t *poll, uint32_t *size);
wstk_status_t wstk_poll_kqueue_space(wstk_poll_kqueue_t *poll, uint32_t *space);
wstk_status_t wstk_poll_kqueue_create(wstk_poll_kqueue_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata);
wstk_status_t wstk_poll_kqueue_add(wstk_poll_kqueue_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_kqueue_del(wstk_poll_kqueue_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_kqueue_polling(wstk_poll_kqueue_t *poll);


/* epoll */
typedef struct wstk_poll_epoll_s wstk_poll_epoll_t;
bool wstk_poll_epoll_is_supported();
bool wstk_poll_epoll_is_empty(wstk_poll_epoll_t *poll);
wstk_status_t wstk_poll_epoll_interrupt(wstk_poll_epoll_t *poll);
wstk_status_t wstk_poll_epoll_size(wstk_poll_epoll_t *poll, uint32_t *size);
wstk_status_t wstk_poll_epoll_space(wstk_poll_epoll_t *poll, uint32_t *space);
wstk_status_t wstk_poll_epoll_create(wstk_poll_epoll_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata);
wstk_status_t wstk_poll_epoll_add(wstk_poll_epoll_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_epoll_del(wstk_poll_epoll_t *poll, wstk_socket_t *socket);
wstk_status_t wstk_poll_epoll_polling(wstk_poll_epoll_t *poll);




#ifdef __cplusplus
}
#endif
#endif
