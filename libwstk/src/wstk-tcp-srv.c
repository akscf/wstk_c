/**
 ** Advanced TCP server
 **
 ** (C)2024 aks
 **/
#include <wstk-tcp-srv.h>
#include <wstk-log.h>
#include <wstk-poll.h>
#include <wstk-net.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-thread.h>
#include <wstk-worker.h>
#include <wstk-queue.h>
#include <wstk-hashtable.h>
#include <wstk-time.h>

#define TCP_SRV_DEFAULT_POLL_TIMEOUT    60  // seconds

struct wstk_tcp_srv_s {
    wstk_mutex_t                *mutex;
    wstk_mutex_t                *mutex_clients;
    wstk_mutex_t                *mutex_attributes;
    wstk_inthash_t              *clients;       // clients id > conn
    wstk_socket_t               *sock;
    wstk_worker_t               *worker_gc;
    wstk_worker_t               *worker_tcp;
    wstk_poll_t                 *poll;
    wstk_hash_t                 *attributes;    // key => attributes_entry_t
    wstk_sockaddr_t             laddr;
    wstk_tcp_srv_handler_t      handler;
    wstk_polling_method_e       polling_method;
    uint32_t                    id;
    uint32_t                    refs;
    uint32_t                    max_conns;
    uint32_t                    max_idle;
    uint32_t                    max_threads;
    uint32_t                    connections;
    uint32_t                    buffer_size;
    bool                        fl_destroyed;
    bool                        fl_ready;
};

struct wstk_tcp_srv_conn_s {
    wstk_mutex_t                *mutex;
    wstk_tcp_srv_t              *server;
    wstk_mbuf_t                 *mbuf;
    wstk_socket_t               *sock;
    wstk_hash_t                 *attributes;    // key => attributes_entry_t
    wstk_sockaddr_t             peer;
    uint32_t                    id;
    uint32_t                    refs;
    bool                        fl_enpolled;    // true when srv-refs been increased
    bool                        fl_destroyed;
    bool                        fl_do_close;
};

typedef struct {
    void    *data;
    bool    auto_destroy;
} attributes_entry_t;

static wstk_status_t srv_refs(wstk_tcp_srv_t *srv);
static void srv_derefs(wstk_tcp_srv_t *srv);

// -----------------------------------------------------------------------------------------------------------------------
static void desctuctor__attributes_entry_t(void *ptr) {
    attributes_entry_t *entry = (attributes_entry_t *)ptr;

    if(!entry) { return; }
#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying attribute: entry=%p (data=%p, auto_destroy=%d)", entry, entry->data, entry->auto_destroy);
#endif
    if(entry->data && entry->auto_destroy) {
        entry->data = wstk_mem_deref(entry->data);
    }

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("attribute destroyed: entry=%p", entry);
#endif
}

static void desctuctor__wstk_tcp_srv_conn_t(void *ptr) {
    wstk_tcp_srv_conn_t *conn = (wstk_tcp_srv_conn_t *)ptr;
    wstk_tcp_srv_t *srv = (conn ? conn->server : NULL);
    bool floop = true;

    if(!conn || !srv || conn->fl_destroyed) {
        return;
    }
    conn->fl_destroyed = true;

    /* interrupt polling */
    if(conn->sock && !conn->sock->fl_destroyed) {
        wstk_sock_close_fd(conn->sock);
    }

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying connection: conn=%p (srv=%p, sock=%p, refs=%d)", conn, srv, conn->sock, conn->refs);
#endif

    /* delete connection from clients map */
    wstk_mutex_lock(srv->mutex_clients);
    wstk_core_inthash_delete(srv->clients, conn->id);
    wstk_mutex_unlock(srv->mutex_clients);

    if(conn->mutex) {
        while(floop) {
            wstk_mutex_lock(conn->mutex);
            floop = (conn->refs > 0);
            wstk_mutex_unlock(conn->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(conn->refs) {
        log_warn("Lost references (refs=%d)", conn->refs);
    }

    if(conn->attributes) {
        wstk_mutex_lock(conn->mutex);
        conn->attributes = wstk_mem_deref(conn->attributes);
        wstk_mutex_unlock(conn->mutex);
    }

    if(conn->fl_enpolled) {
        srv_derefs(srv);
    }

    conn->mbuf = wstk_mem_deref(conn->mbuf);
    conn->sock = wstk_mem_deref(conn->sock);
    conn->mutex = wstk_mem_deref(conn->mutex);

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("connection destroyed: conn=%p", conn);
#endif
}

static void desctuctor__wstk_tcp_srv_t(void *ptr) {
    wstk_tcp_srv_t *srv = (wstk_tcp_srv_t *)ptr;
    bool floop = true;

    if(!srv || srv->fl_destroyed) {
        return;
    }

    srv->fl_ready = false;
    srv->fl_destroyed = true;

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying server: srv=%p (id=0x%x, refs=%d, sock=%p)", srv, srv->id, srv->refs, srv->sock);
#endif

    // interrupt polling
    if(srv->sock) {
        wstk_sock_close_fd(srv->sock);
    }
    if(srv->poll) {
        wstk_poll_interrupt(srv->poll);
    }

    if(srv->mutex) {
        while(floop) {
            wstk_mutex_lock(srv->mutex);
            floop = (srv->refs > 0);
            wstk_mutex_unlock(srv->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(srv->refs) {
        log_warn("Lost references (refs=%d)", srv->refs);
    }

    if(srv->attributes) {
        wstk_mutex_lock(srv->mutex_attributes);
        srv->attributes = wstk_mem_deref(srv->attributes);
        wstk_mutex_unlock(srv->mutex_attributes);
    }

    if(srv->clients) {
        wstk_mutex_lock(srv->mutex_clients);
        srv->clients = wstk_mem_deref(srv->clients);
        wstk_mutex_unlock(srv->mutex_clients);
    }

    srv->sock = wstk_mem_deref(srv->sock);
    srv->worker_gc = wstk_mem_deref(srv->worker_gc);
    srv->worker_tcp = wstk_mem_deref(srv->worker_tcp);
    srv->mutex_clients = wstk_mem_deref(srv->mutex_clients);
    srv->mutex_attributes = wstk_mem_deref(srv->mutex_attributes);
    srv->mutex = wstk_mem_deref(srv->mutex);

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("server destroyed: srv=%p", srv);
#endif
}

static wstk_status_t srv_refs(wstk_tcp_srv_t *srv) {
    if(!srv || srv->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    if(srv->mutex) {
        wstk_mutex_lock(srv->mutex);
        srv->refs++;
        wstk_mutex_unlock(srv->mutex);
    }
    return WSTK_STATUS_SUCCESS;
}
static void srv_derefs(wstk_tcp_srv_t *srv) {
    if(!srv)  { return; }
    if(srv->mutex) {
        wstk_mutex_lock(srv->mutex);
        if(srv->refs > 0) srv->refs--;
        wstk_mutex_unlock(srv->mutex);
    }
}
static wstk_status_t conn_refs(wstk_tcp_srv_conn_t *conn) {
    if(!conn || conn->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    if(conn->mutex) {
        wstk_mutex_lock(conn->mutex);
        conn->refs++;
        wstk_mutex_unlock(conn->mutex);
    }
    return WSTK_STATUS_SUCCESS;
}
static void conn_derefs(wstk_tcp_srv_conn_t *conn) {
    if(!conn)  { return; }
    if(conn->mutex) {
        wstk_mutex_lock(conn->mutex);
        if(conn->refs > 0) conn->refs--;
        wstk_mutex_unlock(conn->mutex);
    }
}

/* called in the polling, reads data from socket and if OK perform it */
static wstk_status_t polling_read_and_perform(wstk_tcp_srv_t *srv, wstk_tcp_srv_conn_t *conn) {
    wstk_status_t st = WSTK_STATUS_NODATA;

    wstk_mbuf_set_pos(conn->mbuf, 0);
    st = wstk_tcp_read(conn->sock, conn->mbuf, 0);
    if(st == WSTK_STATUS_SUCCESS && conn->mbuf->end > 0) {
        conn_refs(conn);
        if((st = wstk_worker_perform(srv->worker_tcp, conn)) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to enqueue connection (conn=%p, sock=%p, st=%d)", conn, conn->sock, (int)st);
            conn_derefs(conn);
        }
    }

    return st;
}

/* the main server thread responsible for polling */
static void polling_thread(wstk_thread_t *th, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_tcp_srv_t *srv = (wstk_tcp_srv_t *)udata;

    if(!srv) {
        log_error("opps! (srv == null)");
        return;
    }

    srv_refs(srv);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("polling-thread started: thread=%p (wait for worker ready...)", th);
#endif

    while(true) {
        if(wstk_worker_is_ready(srv->worker_tcp)) {
            break;
        }
        if(srv->fl_destroyed) {
            break;
        }
        WSTK_SCHED_YIELD(0);
    }
    if(srv->fl_destroyed) {
        goto out;
    }

    /* adding listener socket to the poll */
    wstk_sock_set_expiry(srv->sock, 0);
    wstk_sock_set_pmask(srv->sock, (WSTK_POLL_MREAD | WSTK_POLL_MLISTENER));

    if((status = wstk_poll_add(srv->poll, srv->sock)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    srv->fl_ready = true;
    while(!srv->fl_destroyed) {
        if(wstk_poll_is_empty(srv->poll))  {
            wstk_msleep(1000);
        } else {
            wstk_poll_polling(srv->poll);
        }
    }

out:
    srv->fl_ready = false;

    if(status != WSTK_STATUS_SUCCESS) {
        log_warn("terminated with status: %d", (int)status);
    }

    /* destroyng poll */
    srv->poll = wstk_mem_deref(srv->poll);

    srv_derefs(srv);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("polling-thread finished: thread=%p", th);
#endif
}

static void poll_handler(wstk_socket_t *socket, int event, void *udata) {
    wstk_tcp_srv_t *srv = (wstk_tcp_srv_t *)udata;

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("poll_handler: srv=%p, listener-sock=%p, curr-sock=%p, conn=%p, event=0x%x [EREAD=%d, EWRITE=%d, ECLOSED=%d, ESEXPIED=%d]",
                srv, srv->sock, socket, socket->udata, event,
                event & WSTK_POLL_EREAD,
                event & WSTK_POLL_EWRITE,
                event & WSTK_POLL_ESCLOSED,
                event & WSTK_POLL_ESEXPIRED
    );
#endif

    if(event & WSTK_POLL_ESCLOSED || event & WSTK_POLL_ESEXPIRED || srv->fl_destroyed) {
        wstk_tcp_srv_conn_t *conn = (wstk_tcp_srv_conn_t *)socket->udata;

        /* delete socket from the poll */
        wstk_poll_del(srv->poll, socket);

        /* don't touch the listener socket,                 */
        /* this will be destroyed in the server destructor  */
        if(socket->pmask & WSTK_POLL_MLISTENER) {
            if(srv->connections) {
                srv->connections--;
            }
            return;
        }

        /* using gc for the locked connections */
        if(conn && conn->refs > 0) {
            if(wstk_worker_perform(srv->worker_gc, socket) != WSTK_STATUS_SUCCESS) {
                wstk_mem_deref(socket);
            }
        } else {
            wstk_mem_deref(socket);
        }

        if(srv->connections) {
            srv->connections--;
        }

        return;
    }

    if(srv->sock == socket) {
        wstk_socket_t *csock = NULL;
        wstk_tcp_srv_conn_t *conn = NULL;

        if(wstk_tcp_accept(socket, &csock, 0) == WSTK_STATUS_SUCCESS) {
            if(srv->max_conns && srv->connections >= srv->max_conns) {
                log_error("Too many connections (rejected)");
                wstk_mem_deref(csock);
                return;
            }
            if(wstk_mem_zalloc((void *)&conn, sizeof(wstk_tcp_srv_conn_t), desctuctor__wstk_tcp_srv_conn_t) != WSTK_STATUS_SUCCESS) {
                log_error("Unable to allocate memory");
                wstk_mem_deref(csock);
                return;
            }
            if(wstk_mutex_create(&conn->mutex) != WSTK_STATUS_SUCCESS) {
                log_error("Unable to create mutex");
                wstk_mem_deref(conn);
                wstk_mem_deref(csock);
                return;
            }
            if(wstk_mbuf_alloc(&conn->mbuf, srv->buffer_size) != WSTK_STATUS_SUCCESS) {
                log_error("Unable to allocate memory");
                wstk_mem_deref(conn);
                wstk_mem_deref(csock);
                return;
            }
            if(wstk_hash_init(&conn->attributes) != WSTK_STATUS_SUCCESS) {
                log_error("Unable to create attributes");
                wstk_mem_deref(conn);
                wstk_mem_deref(csock);
                return;
            }

            conn->sock = csock;
            conn->server = srv;
            wstk_sock_get_peer(csock, &conn->peer);
            wstk_sa_hash(&conn->peer, &conn->id);

            wstk_sock_set_udata(csock, conn, true);
            wstk_sock_set_pmask(csock, WSTK_POLL_MREAD);
            wstk_sock_set_expiry(csock, srv->max_idle);

            /* add connection into clients map */
            wstk_mutex_lock(srv->mutex_clients);
            wstk_inthash_insert(srv->clients, conn->id, conn);
            wstk_mutex_unlock(srv->mutex_clients);

            if(wstk_poll_add(srv->poll, csock) != WSTK_STATUS_SUCCESS) {
                log_error("Unable to add socket to the poll");
                wstk_mem_deref(conn);
            } else {
                srv_refs(srv);
                srv->connections++;
                conn->fl_enpolled = true;

                polling_read_and_perform(srv, conn);
            }
        }
        return;
    }

    if(event && WSTK_POLL_EREAD) {
        wstk_tcp_srv_conn_t *conn = (wstk_tcp_srv_conn_t *)socket->udata;

        /* always udaptes expiry */
        wstk_sock_set_expiry(conn->sock, conn->server->max_idle);

        if(!conn->refs) {
            polling_read_and_perform(srv, conn);
        }
    }
}

/* called by worker_gc */
static void gc_worker_handler(wstk_worker_t *worker, void *qdata) {
        wstk_socket_t *sock = (wstk_socket_t *)qdata;

#ifdef WSTK_TCP_SRV_DEBUG
        WSTK_DBG_PRINT("gc: socket=%p", sock);
#endif
        wstk_mem_deref(sock);
}

/* called by worker_tcp */
static void tcp_worker_handler(wstk_worker_t *worker, void *data) {
    wstk_tcp_srv_conn_t *conn = (wstk_tcp_srv_conn_t *)data;
    wstk_tcp_srv_t *srv = conn->server;

    if(!srv->fl_destroyed) {
        srv->handler(conn, conn->mbuf);
        if(!conn->fl_destroyed && !conn->sock->fl_destroyed) {
            if(conn->fl_do_close) {
                shutdown(conn->sock->fd, SHUT_RDWR);
            }
        }
    }

    conn_derefs(conn);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new instance
 *
 * @param srv           - a new server
 * @param address       - address for listening
 * @param max_conns     - max connections (default: 1024)
 * @param max_idle      - max connections idle (in seconds, default: 60s)
 * @param buffer_size   - read buffer size (default: 8192)
 * @param handler       - messags processing handler
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_create(wstk_tcp_srv_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t max_idle, uint32_t buffer_size, wstk_tcp_srv_handler_t handler) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_tcp_srv_t *srv_local = NULL;
    uint32_t poll_size = 0, poll_timeout = 0;
    wstk_poll_auto_conf_t poll_aconf = { 0 };

    if(!srv || !address || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&srv_local, sizeof(wstk_tcp_srv_t), desctuctor__wstk_tcp_srv_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&srv_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_mutex_create(&srv_local->mutex_clients)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_mutex_create(&srv_local->mutex_attributes)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_inthash_init(&srv_local->clients)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    wstk_sa_init(&srv_local->laddr, AF_UNSPEC);
    if((status = wstk_sa_cpy(&srv_local->laddr, address)) != WSTK_STATUS_SUCCESS) {
       goto out;
    }

    wstk_sa_hash(&srv_local->laddr, &srv_local->id);

    srv_local->handler = handler;
    srv_local->max_idle = (max_idle ? max_idle : 60);
    srv_local->max_conns = (max_conns ? max_conns : 1023);      // +1 for listener
    srv_local->buffer_size = (buffer_size ? buffer_size : 8192);
    srv_local->polling_method = ((srv_local->max_conns + 1) <= 1024 ? WSTK_POLL_SELECT : WSTK_POLL_AUTO);

    /* poll auto-conf */
    poll_size = (srv_local->max_conns + 1);
    poll_timeout = (srv_local->max_idle < TCP_SRV_DEFAULT_POLL_TIMEOUT ? srv_local->max_idle : TCP_SRV_DEFAULT_POLL_TIMEOUT);

    poll_aconf = wstk_poll_auto_conf(srv_local->polling_method, poll_size);
    if(poll_aconf.size < poll_size) {
        poll_size = poll_size;
        srv_local->max_conns = (poll_aconf.size - 1);

        log_warn("Selected polling method doesn't support such size (max_conns been truncated to: %d)", srv_local->max_conns);
    }

    srv_local->polling_method = poll_aconf.method;
    srv_local->max_threads = srv_local->max_conns;

    /* workers and polls */
    status = wstk_worker_create(&srv_local->worker_gc, 1, 10, srv_local->max_conns, 25, gc_worker_handler);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_worker_create(&srv_local->worker_tcp, 3, srv_local->max_threads, (srv_local->max_conns + 64), 45, tcp_worker_handler);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_poll_create(&srv_local->poll, srv_local->polling_method, poll_size, poll_timeout, 0x0, poll_handler, srv_local);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_hash_init(&srv_local->attributes);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    *srv = srv_local;

#ifdef WSTK_TCP_SRV_DEBUG
    WSTK_DBG_PRINT("server created: server=%p (id=0x%x, max_conns=%d, max_threads=%d, max_idle=%d, buffer_size=%d, polling_method=%d)",
                   srv_local, srv_local->id, srv_local->max_conns, srv_local->max_threads, srv_local->max_idle, srv_local->buffer_size, srv_local->polling_method);
#endif
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(srv_local);
    }
    return status;
}

/**
 * Create a new instance (ssl)
 *
 * @param srv           - a new server
 * @param address       - address for listening
 * @param cert          - certificate file
 * @param max_conns     - max connections (default: 1024)
 * @param max_idle      - max connections idle (in seconds, default: 60s)
 * @param buffer_size   - read buffer size (default: 8192)
 * @param handler       - messags processing handler
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_ssl_srv_create(wstk_tcp_srv_t **srv, wstk_sockaddr_t *address, char *cert, uint32_t max_conns, uint32_t max_idle, uint32_t buffer_size, wstk_tcp_srv_handler_t handler) {

    //
    // todo
    //

    return WSTK_STATUS_NOT_IMPL;
}

/**
 * Start server instance
 *
 * @param srv - the server instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_start(wstk_tcp_srv_t *srv) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }
    if(srv->fl_ready) {
        return WSTK_STATUS_SUCCESS;
    }

    status = wstk_tcp_listen(&srv->sock, &srv->laddr, 5);
    if(status == WSTK_STATUS_SUCCESS) {
        status = wstk_thread_create(NULL, polling_thread, srv, 0x0);
    } else {
        log_error("Unable to start listener (status=%d)", (int) status);
    }

#ifdef WSTK_TCP_SRV_DEBUG
    if(status == WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("server started: srv=%p", srv);
    }
#endif

    return status;
}


/**
 * Get instance id
 *
 * @param srv   - the server instance
 * @param id    - the id
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_id(wstk_tcp_srv_t *srv, uint32_t *id) {
    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *id = srv->id;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get polling method
 *
 * @param srv       - the server instance
 * @param method    - the method
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_polling_method(wstk_tcp_srv_t *srv, wstk_polling_method_e *method) {
    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *method = srv->polling_method;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get listening address
 *
 * @param srv       - the server instance
 * @param laddr     - the laddr
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_listen_address(wstk_tcp_srv_t *srv, wstk_sockaddr_t **laddr) {
    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *laddr = (wstk_sockaddr_t *)&srv->laddr;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get server ready flag
 *
 * @param srv   - the server
 *
 * @return true/false
 **/
bool wstk_tcp_srv_is_ready(wstk_tcp_srv_t *srv) {
    if(!srv || srv->fl_destroyed) {
        return false;
    }
    return srv->fl_ready;
}

/**
 * Get server life state
 *
 * @param srv   - the server
 *
 * @return true/false
 **/
bool wstk_tcp_srv_is_destroyed(wstk_tcp_srv_t *srv) {
    if(!srv) {
        return false;
    }
    return srv->fl_destroyed;
}


/**
 * Close connection
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_close(wstk_tcp_srv_conn_t *conn) {
    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    conn->fl_do_close = true;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Is be destroyed
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
bool wstk_tcp_srv_conn_is_destroyed(wstk_tcp_srv_conn_t *conn) {
    if(!conn || conn->fl_destroyed) {
        return true;
    }
    return conn->fl_destroyed;
}

/**
 * Is be closed
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
bool wstk_tcp_srv_conn_is_closed(wstk_tcp_srv_conn_t *conn) {
    if(!conn) {
        return false;
    }
    if(conn->fl_destroyed || conn->fl_do_close) {
        return true;
    }
    return false;
}

/**
 * Get connection id
 *
 * @param conn  - connection
 * @param id    - unique id
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_id(wstk_tcp_srv_conn_t *conn, uint32_t *id) {
    if(!conn || !id) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *id = conn->id;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get peer address
 *
 * @param conn  - connection
 * @param peer  - reference to the peer address (DON'T destroy it in the handler!)
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_peer(wstk_tcp_srv_conn_t *conn, wstk_sockaddr_t **peer) {
    if(!conn || !peer) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *peer = (wstk_sockaddr_t *)&conn->peer;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get client socket
 *
 * @param conn  - connection
 * @param sock  - reference to the socket (DON'T destroy it in the handler!)
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_socket(wstk_tcp_srv_conn_t *conn, wstk_socket_t **sock) {
    if(!conn || !sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *sock = conn->sock;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get server
 *
 * @param conn  - connection
 * @param srv   - reference to the server
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_server(wstk_tcp_srv_conn_t *conn, wstk_tcp_srv_t **srv) {
    if(!conn || !srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *srv = conn->server;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Read data
 *
 *
 * @param srv       - the server instance
 * @param mbuf      - mbuf or null to use internal buffer
 * @param timeout   - 0 or timeout in seconds
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_read(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_tcp_srv_t *srv = (conn ? conn->server : NULL);

    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(!mbuf || mbuf == conn->mbuf) {
        status = wstk_tcp_read(conn->sock, conn->mbuf, timeout);
    } else {
        status = wstk_tcp_read(conn->sock, mbuf, timeout);
    }

    /* check and udapte expiry */
    if(status == WSTK_STATUS_SUCCESS) {
        wstk_sock_set_expiry(conn->sock, srv->max_idle);
    } else {
        if(conn->sock->expiry && conn->sock->expiry <= wstk_time_epoch_now()) {
            status = WSTK_STATUS_CONN_EXPIRE;
        }
    }

    if(status == WSTK_STATUS_CONN_DISCON || status == WSTK_STATUS_CONN_EXPIRE) {
        conn->fl_do_close = true;
    }

    return status;
}


/**
 * Write data
 *
 * @param srv       - the server instance
 * @param mbuf      - mbuf or null for use connection buffer
 * @param timeout   - 0 or timeout in seconds
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_write(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!conn || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    status = wstk_tcp_write(conn->sock, mbuf, timeout);
    if(status == WSTK_STATUS_CONN_DISCON) {
        conn->fl_do_close = true;
    }

    return status;
}

/**
 * Put attribute
 *
 * @param srv           - the server instance
 * @param name          - attr name
 * @param value         - attr value
 * @param auto_destroy  - will be destroyed on hash delete
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_attr_add(wstk_tcp_srv_t *srv, const char *name, void *value, bool auto_destroy) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex_attributes);
    entry = wstk_hash_find(srv->attributes, name);
    if(entry) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
        status = wstk_mem_zalloc((void *)&entry, sizeof(attributes_entry_t), desctuctor__attributes_entry_t);
        if(status == WSTK_STATUS_SUCCESS) {
            entry->data = value;
            entry->auto_destroy = auto_destroy;

            status = wstk_hash_insert_ex(srv->attributes, name, entry, true);
            if(status != WSTK_STATUS_SUCCESS) {
                wstk_mem_deref(entry);
            }
        }
    }
    wstk_mutex_unlock(srv->mutex_attributes);

#ifdef WSTK_TCP_SRV_DEBUG
    if(status == WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("attribute added: entry=%p (name=%s, data=%p, auto_destroy=%d)", entry, name, entry->data, entry->auto_destroy);
    }
#endif

    return status;
}

/**
 * Delete attribute
 *
 * @param srv           - the server instance
 * @param name          - attr name
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_attr_del(wstk_tcp_srv_t *srv, const char *name) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex_attributes);
    wstk_hash_delete(srv->attributes, name);
    wstk_mutex_unlock(srv->mutex_attributes);

    return status;
}

/**
 * Get attribute
 *
 * @param srv           - the server instance
 * @param name          - attr name
 * @param value         - attr value
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_attr_get(wstk_tcp_srv_t *srv, const char *name, void **value) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex_attributes);
    entry = wstk_hash_find(srv->attributes, name);
    if(entry) {
        *value = entry->data;
    } else {
        *value = NULL;
        status = WSTK_STATUS_NOT_FOUND;
    }
    wstk_mutex_unlock(srv->mutex_attributes);

    return status;
}

/**
 * Put attribute
 *
 * @param conn          - the connection
 * @param name          - attr name
 * @param value         - attr value
 * @param auto_destroy  - will be destroyed on hash delete
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_attr_add(wstk_tcp_srv_conn_t *conn, const char *name, void *value, bool auto_destroy) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!conn || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(conn->mutex);
    entry = wstk_hash_find(conn->attributes, name);
    if(entry) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
        status = wstk_mem_zalloc((void *)&entry, sizeof(attributes_entry_t), desctuctor__attributes_entry_t);
        if(status == WSTK_STATUS_SUCCESS) {
            entry->data = value;
            entry->auto_destroy = auto_destroy;

            status = wstk_hash_insert_ex(conn->attributes, name, entry, true);
            if(status != WSTK_STATUS_SUCCESS) {
                wstk_mem_deref(entry);
            }
        }
    }
    wstk_mutex_unlock(conn->mutex);

#ifdef WSTK_TCP_SRV_DEBUG
    if(status == WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("attribute added: entry=%p (name=%s, data=%p, auto_destroy=%d)", entry, name, entry->data, entry->auto_destroy);
    }
#endif

    return status;
}

/**
 * Delete attribute
 *
 * @param conn          - the connection
 * @param name          - attr name
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_attr_del(wstk_tcp_srv_conn_t *conn, const char *name) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!conn || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(conn->mutex);
    wstk_hash_delete(conn->attributes, name);
    wstk_mutex_unlock(conn->mutex);

    return status;
}

/**
 * Get attribute
 *
 * @param conn          - the connection
 * @param name          - attr name
 * @param value         - attr value
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_attr_get(wstk_tcp_srv_conn_t *conn, const char *name, void **value) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!conn || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(conn->mutex);
    entry = wstk_hash_find(conn->attributes, name);
    if(entry) {
        *value = entry->data;
    } else {
        *value = NULL;
        status = WSTK_STATUS_NOT_FOUND;
    }
    wstk_mutex_unlock(conn->mutex);

    return status;
}

/**
 * Lookup client connection (it will be locked if found)
 * Don't forget about: wstk_tcp_srv_conn_release()
 *
 * @param srv      - the server instance
 * @param id       - connection id
 *
 * @return conn or NULL
 **/
wstk_tcp_srv_conn_t *wstk_tcp_srv_lookup_conn(wstk_tcp_srv_t *srv, uint32_t id) {
    wstk_tcp_srv_conn_t *conn = NULL;

    if(!srv || srv->fl_destroyed) {
        return NULL;
    }

    wstk_mutex_lock(srv->mutex_clients);
    conn = wstk_core_inthash_find(srv->clients, id);
    if(conn) {
        if(conn_refs(conn) != WSTK_STATUS_SUCCESS) {
            conn = NULL;
        }
    }
    wstk_mutex_unlock(srv->mutex_clients);

    return conn;
}

/**
 * Marked connection as taken
 * (increase refs)
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_take(wstk_tcp_srv_conn_t *conn) {
    return conn_refs(conn);
}

/**
 * Release connection
 * (decreased refs)
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_release(wstk_tcp_srv_conn_t *conn) {
    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    conn_derefs(conn);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Protect the socket from reading on the polling
 * this only set/clear flag: WSTK_POLL_MRDLOCK
 *
 * @param conn  - the connection
 * @param flag  - true/false
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_tcp_srv_conn_rdlock(wstk_tcp_srv_conn_t *conn, bool flag) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    uint32_t pmask = 0;

    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed || conn->sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(conn->mutex);
    pmask = conn->sock->pmask;
    if(flag) {
        if(pmask & WSTK_POLL_MRDLOCK) {
            status = WSTK_STATUS_FALSE;
        } else {
            conn->sock->pmask |= WSTK_POLL_MRDLOCK;
        }
    } else {
        conn->sock->pmask ^= (pmask & WSTK_POLL_MRDLOCK);
    }
    wstk_mutex_unlock(conn->mutex);

    return status;
}
