/**
 ** Advanced UDP server
 **
 ** (C)2024 aks
 **/
#include <wstk-udp-srv.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-net.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-thread.h>
#include <wstk-worker.h>
#include <wstk-hashtable.h>
#include <wstk-time.h>

struct wstk_udp_srv_s {
    wstk_mutex_t                *mutex;
    wstk_socket_t               *sock;
    wstk_worker_t               *worker;
    wstk_hash_t                 *attributes;    // key => attributes_entry_t
    wstk_sockaddr_t             laddr;
    wstk_udp_srv_handler_t      handler;
    uint32_t                    id;
    uint32_t                    refs;
    uint32_t                    max_conns;
    uint32_t                    max_threads;
    uint32_t                    connections;
    uint32_t                    buffer_size;
    bool                        fl_destroyed;
    bool                        fl_ready;
};

struct wstk_udp_srv_conn_s {
    wstk_mutex_t                *mutex;
    wstk_udp_srv_t              *server;
    wstk_mbuf_t                 *mbuf;
    void                        *udata;
    uint32_t                    id;
    wstk_sockaddr_t             peer;
    bool                        fl_enqueued;
    bool                        fl_destroyed;
    bool                        fl_adestroy_udata;
};

typedef struct {
    void        *data;
    bool        auto_destroy;
} attributes_entry_t;

static void desctuctor__attributes_entry_t(void *ptr) {
    attributes_entry_t *entry = (attributes_entry_t *)ptr;

    if(!entry) { return; }

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying attribute: entry=%p (data=%p, auto_destroy=%d)", entry, entry->data, entry->auto_destroy);
#endif
    if(entry->data && entry->auto_destroy) {
        entry->data = wstk_mem_deref(entry->data);
    }

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("attribute destroyed: entry=%p", entry);
#endif
}

static void desctuctor__wstk_udp_srv_conn_t(void *ptr) {
    wstk_udp_srv_conn_t *conn = (wstk_udp_srv_conn_t *)ptr;
    wstk_udp_srv_t *srv = (conn ? conn->server : NULL);

    if(!conn || !srv || conn->fl_destroyed) {
        return;
    }

    conn->fl_destroyed = true;

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying connection: conn=%p (srv=%p)", conn, srv);
#endif

    if(conn->fl_enqueued) {
        wstk_mutex_lock(srv->mutex);
        if(srv->refs) srv->refs--;
        if(srv->connections) srv->connections--;
        wstk_mutex_unlock(srv->mutex);
    }

    if(conn->udata && conn->fl_adestroy_udata) {
        conn->udata = wstk_mem_deref(conn->udata);
    }

    conn->mbuf = wstk_mem_deref(conn->mbuf);
    conn->mutex = wstk_mem_deref(conn->mutex);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("connection destroyed: conn=%p", conn);
#endif
}

static void desctuctor__wstk_udp_srv_t(void *ptr) {
    wstk_udp_srv_t *srv = (wstk_udp_srv_t *)ptr;
    bool floop = true;

    if(!srv || srv->fl_destroyed) {
        return;
    }

    srv->fl_ready = false;
    srv->fl_destroyed = true;

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("destroying server: srv=%p (id=0x%x, refs=%d, sock=%p)", srv, srv->id, srv->refs, srv->sock);
#endif

    // interrupt polling
    if(srv->sock) {
        wstk_sock_close_fd(srv->sock);
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
        srv->attributes = wstk_mem_deref(srv->attributes);
    }

    srv->sock = wstk_mem_deref(srv->sock);
    srv->worker = wstk_mem_deref(srv->worker);
    srv->mutex = wstk_mem_deref(srv->mutex);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("server destroyed: srv=%p", srv);
#endif
}

static void polling_thread(wstk_thread_t *th, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_udp_srv_t *srv = (wstk_udp_srv_t *)udata;
    wstk_mbuf_t *mbuf = NULL;
    struct timeval tv = { 0 };
    int err=0, herr=0;
    fd_set rdset;

    if(!srv) {
        log_error("opps! (srv == null)");
        return;
    }

    wstk_mutex_lock(srv->mutex);
    srv->refs++;
    wstk_mutex_unlock(srv->mutex);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("polling-thread started: thread=%p (wait for worker ready...)", th);
#endif

    while(true) {
        if(wstk_worker_is_ready(srv->worker)) {
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

    if(wstk_mbuf_alloc(&mbuf, srv->buffer_size) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to allocate memory");
        goto out;
    }

    srv->fl_ready = true;
    while(!srv->fl_destroyed) {
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        FD_ZERO(&rdset);
        FD_SET(srv->sock->fd, &rdset);

        if((err = select(srv->sock->fd + 1, &rdset, NULL, NULL, &tv)) <= 0) {
            if(err < 0) { herr = err; break;}
            continue;
        }
        if(srv->fl_destroyed) {
            break;
        }

        if(FD_ISSET(srv->sock->fd, &rdset)) {
            wstk_sockaddr_t paddr = { 0 };
            wstk_udp_srv_conn_t *conn = NULL;

            wstk_mbuf_set_pos(mbuf, 0);
            wstk_mbuf_set_end(mbuf, 0);

            if(wstk_udp_recv(srv->sock, &paddr, mbuf, 0) == WSTK_STATUS_SUCCESS && wstk_mbuf_pos(mbuf) > 0) {
                if(srv->max_conns && srv->connections >= srv->max_conns) {
                    log_warn("Request's ignored (too many connections)");
                    continue;
                }

                if(wstk_mem_zalloc((void *)&conn, sizeof(wstk_udp_srv_conn_t), desctuctor__wstk_udp_srv_conn_t) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to allocate memory");
                    continue;
                }
                if(wstk_mutex_create(&conn->mutex) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to create mutex");
                    wstk_mem_deref(conn);
                    continue;
                }

                conn->server = srv;
                wstk_sa_init(&conn->peer, AF_UNSPEC);
                if(wstk_sa_cpy(&conn->peer, &paddr) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to copy peer address");
                    wstk_mem_deref(conn);
                    continue;
                }
                wstk_sa_hash(&conn->peer, &conn->id);

                wstk_mbuf_set_pos(mbuf, 0);
                if(wstk_mbuf_dup(&conn->mbuf, mbuf) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to allocate memory");
                    wstk_mem_deref(conn);
                    continue;
                }

                wstk_mutex_lock(srv->mutex);
                srv->refs++;
                srv->connections++;
                wstk_mutex_unlock(srv->mutex);

                conn->fl_enqueued = true;
                wstk_mbuf_set_pos(conn->mbuf, 0);

                if(wstk_worker_perform(srv->worker, conn) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to enqueue connection");
                    wstk_mem_deref(conn);
                    continue;
                }
            }
        }
    }
out:
    if(herr) {
        log_warn("select failed (err=%d)", herr);
    }

    wstk_mem_deref(mbuf);

    wstk_mutex_lock(srv->mutex);
    if(srv->refs) srv->refs--;
    wstk_mutex_unlock(srv->mutex);

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("polling-thread finished: thread=%p", th);
#endif
}

static void worker_handler(wstk_worker_t *worker, void *data) {
    wstk_udp_srv_conn_t *conn = (wstk_udp_srv_conn_t *)data;
    wstk_udp_srv_t *srv = (conn ? conn->server : NULL);

    if(conn && srv && srv->fl_ready) {
        srv->handler(conn, conn->mbuf);
    }
    wstk_mem_deref(conn);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new instance
 *
 * @param srv           - a new server
 * @param address       - address for listening
 * @param max_conns     - max connections
 * @param buffer_size   - read buffer size (default: 8192)
 * @param handler       - messags processing handler
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_create(wstk_udp_srv_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t buffer_size, wstk_udp_srv_handler_t handler) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_udp_srv_t *srv_local = NULL;

    if(!srv || !address || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&srv_local, sizeof(wstk_udp_srv_t), desctuctor__wstk_udp_srv_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_mutex_create(&srv_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_hash_init(&srv_local->attributes)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    wstk_sa_init(&srv_local->laddr, AF_UNSPEC);
    if((status = wstk_sa_cpy(&srv_local->laddr, address)) != WSTK_STATUS_SUCCESS) {
       goto out;
    }

    wstk_sa_hash(&srv_local->laddr, &srv_local->id);

    srv_local->handler = handler;
    srv_local->max_conns = (max_conns ? max_conns : 1024);
    srv_local->max_threads = srv_local->max_conns;
    srv_local->buffer_size = (buffer_size ? buffer_size : 8192);

    status = wstk_worker_create(&srv_local->worker, 3, srv_local->max_threads, (srv_local->max_conns + 64), 45, worker_handler);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    *srv = srv_local;

#ifdef WSTK_UDP_SRV_DEBUG
    WSTK_DBG_PRINT("server created: server=%p (id=0x%x, max_conns=%d, max_threads=%d, buff_size=%d)", srv_local, srv_local->id, srv_local->max_conns, srv_local->max_threads, srv_local->buffer_size);
#endif

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(srv_local);
    }
    return status;
}

/**
 * Start server instance
 *
 * @param srv - the server instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_start(wstk_udp_srv_t *srv) {
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

    status = wstk_udp_listen(&srv->sock, &srv->laddr);
    if(status == WSTK_STATUS_SUCCESS) {
        status = wstk_thread_create(NULL, polling_thread, srv, 0x0);
    } else {
        log_error("Unable to start listener (status=%d)", (int) status);
    }

#ifdef WSTK_UDP_SRV_DEBUG
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
wstk_status_t wstk_udp_srv_id(wstk_udp_srv_t *srv, uint32_t *id) {
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
 * Get listening address
 *
 * @param srv       - the server instance
 * @param laddr     - the laddr
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_listen_address(wstk_udp_srv_t *srv, wstk_sockaddr_t **laddr) {
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
bool wstk_udp_srv_is_ready(wstk_udp_srv_t *srv) {
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
bool wstk_udp_srv_is_destroyed(wstk_udp_srv_t *srv) {
    if(!srv) {
        return false;
    }
    return srv->fl_destroyed;
}

/**
 * Get connection id
 *
 * @param conn  - connection
 * @param id    - unique id
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_conn_id(wstk_udp_srv_conn_t *conn, uint32_t *id) {
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
 * @param srv   - the server
 * @param peer  - reference to the peer address (DON'T destroy it in the handler!)
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_conn_peer(wstk_udp_srv_conn_t *conn, wstk_sockaddr_t **peer) {
    if(!peer || !conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *peer = (wstk_sockaddr_t *)&conn->peer;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Set user data
 *
 * @param srv           - the server
 * @param udata         - some data
 * @param auto_destroy  - destroy when socket closed
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_conn_set_udata(wstk_udp_srv_conn_t *conn, void *udata, bool auto_destroy) {
    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    conn->udata = udata;
    conn->fl_adestroy_udata = (udata ? auto_destroy : false);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Get user data
 *
 * @param srv           - the server
 * @param udata         - some data
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_conn_udata(wstk_udp_srv_conn_t *conn, void **udata) {
    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *udata = conn->udata;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Write message to the peer
 *
 * @param srv   - the server
 * @param mbuf  - buffer to send
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_udp_srv_write(wstk_udp_srv_conn_t *conn, wstk_mbuf_t *mbuf) {
    wstk_udp_srv_t *srv = (conn ? conn->server : NULL);

    if(!conn || !srv || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->fl_destroyed || srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    return wstk_udp_send(srv->sock, &conn->peer, mbuf);
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
wstk_status_t wstk_udp_srv_attr_add(wstk_udp_srv_t *srv, const char *name, void *value, bool auto_destroy) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex);
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
    wstk_mutex_unlock(srv->mutex);

#ifdef WSTK_UDP_SRV_DEBUG
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
wstk_status_t wstk_udp_srv_attr_del(wstk_udp_srv_t *srv, const char *name) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex);
    wstk_hash_delete(srv->attributes, name);
    wstk_mutex_unlock(srv->mutex);

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
wstk_status_t wstk_udp_srv_attr_get(wstk_udp_srv_t *srv, const char *name, void **value) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    attributes_entry_t *entry = NULL;

    if(!srv || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex);
    entry = wstk_hash_find(srv->attributes, name);
    if(entry) {
        *value = entry->data;
    } else {
        *value = NULL;
        status = WSTK_STATUS_NOT_FOUND;
    }
    wstk_mutex_unlock(srv->mutex);

    return status;
}
