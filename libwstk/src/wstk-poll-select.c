/**
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-poll.h>
#include <wstk-mem.h>
#include <wstk-sleep.h>
#include <wstk-thread.h>
#include <wstk-hashtable.h>
#include <wstk-list.h>
#include <wstk-time.h>

struct wstk_poll_select_s {
    wstk_inthash_t          *sockets;
    wstk_list_t             *slist1;
    wstk_list_t             *slist2;
    void                    *udata;
    wstk_poll_handler_t     handler;
    uint32_t                size;
    uint32_t                timeout;
    uint32_t                flags;
    bool                    fl_polling;
    bool                    fl_destroyed;
};

typedef struct {
    wstk_poll_select_t  *poll;
    wstk_socket_t       *socket;
    int                 act;
} slist_entry_t;

static void destructor__wstk_poll_select_t(void *data) {
    wstk_poll_select_t *poll = (wstk_poll_select_t *)data;

    if(!poll || poll->fl_destroyed) {
        return;
    }
    poll->fl_destroyed = true;

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("destroying poll: poll=%p", poll);
#endif

    if(poll->fl_polling) {
        while(poll->fl_polling) {
            WSTK_SCHED_YIELD(0);
        }
    }

    if(poll->sockets) {
        if(!wstk_hash_is_empty(poll->sockets)) {
            wstk_hash_index_t *hidx = NULL;
            wstk_socket_t *sock = NULL;

            for(hidx = wstk_hash_first_iter(poll->sockets, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
                wstk_hash_this(hidx, NULL, NULL, (void *)&sock);

                if(!sock) { continue; }

                wstk_core_inthash_delete(poll->sockets, sock->fd);
                poll->handler(sock, WSTK_POLL_ESCLOSED, poll->udata);
            }
        }
        poll->sockets = wstk_mem_deref(poll->sockets);
    }

    poll->slist1 = wstk_mem_deref(poll->slist1);
    poll->slist2 = wstk_mem_deref(poll->slist2);

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("poll destroyed: poll=%p ", poll);
#endif
}

static wstk_status_t poll_socket_add_perform(wstk_poll_select_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(wstk_hash_size(poll->sockets) >= poll->size) {
        return WSTK_STATUS_NOSPACE;
    }

    if(wstk_core_inthash_find(poll->sockets, socket->fd)) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
        status = wstk_inthash_insert(poll->sockets, socket->fd, socket);
    }

    return status;
}

static wstk_status_t poll_socket_del_perform(wstk_poll_select_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(wstk_core_inthash_delete(poll->sockets, socket->fd)) {
        status = WSTK_STATUS_SUCCESS;
    }

    return status;
}

static wstk_status_t poll_socket_deferred_action(wstk_poll_select_t *poll, wstk_socket_t *socket, int act) {
    wstk_status_t status = WSTK_STATUS_FALSE;
    slist_entry_t *entry = NULL;

    status = wstk_mem_zalloc((void *)&entry, sizeof(slist_entry_t), NULL);
    if(status == WSTK_STATUS_SUCCESS) {
        entry->poll = poll;
        entry->socket = socket;
        entry->act = act;

        if(act == 1) {
            if((status = wstk_list_add_head(poll->slist1, entry, NULL)) != WSTK_STATUS_SUCCESS) {
                wstk_mem_deref(entry);
            }
        } else if(act == 2) {
            if((status = wstk_list_add_head(poll->slist2, entry, NULL)) != WSTK_STATUS_SUCCESS) {
                wstk_mem_deref(entry);
            } else {
                wstk_mem_ref(socket); // always do (for protect to destroy on disconnect)
            }
        }
    }

    return status;
}

static void slist_action_perform_cb(uint32_t pos, void *data) {
    slist_entry_t *entry = (slist_entry_t *)data;
    wstk_status_t st = 0;

    if(entry) {
        if(entry->act == 1) { // add
            st = poll_socket_add_perform(entry->poll, entry->socket);
            if(st != WSTK_STATUS_SUCCESS) {
                log_error("Unable to add socket to the pool (poll=%p, sock=%p, status=%d)", entry->poll, entry->socket, (int)st);
                wstk_mem_deref(entry->socket);
            }
        } else if(entry->act == 2) { // del
            st = poll_socket_del_perform(entry->poll, entry->socket);
            wstk_mem_deref(entry->socket); // always do
        }
        wstk_mem_deref(entry);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool wstk_poll_select_is_supported() {
    return true;
}

uint32_t wstk_poll_select_max_size() {
    return FD_SETSIZE;
}

bool wstk_poll_select_is_empty(wstk_poll_select_t *poll) {
    if(!poll || poll->fl_destroyed) {
        return false;
    }
    return wstk_hash_is_empty(poll->sockets);
}

wstk_status_t wstk_poll_select_space(wstk_poll_select_t *poll, uint32_t *space) {
    if(!poll || !space) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *space = (poll->size - wstk_hash_size(poll->sockets));
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_poll_select_size(wstk_poll_select_t *poll, uint32_t *size) {
    if(!poll || !size) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

   *size = poll->size;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_poll_select_create(wstk_poll_select_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_poll_select_t *pvt = NULL;

    if(!poll || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&pvt, sizeof(wstk_poll_select_t), destructor__wstk_poll_select_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_inthash_init(&pvt->sockets)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_list_create(&pvt->slist1)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_list_create(&pvt->slist2)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(size > FD_SETSIZE)  {
        size = FD_SETSIZE;
    }

    pvt->size = (size ? size : FD_SETSIZE);
    pvt->timeout = (timeout ? timeout : 60);
    pvt->flags = flags;
    pvt->udata = udata;
    pvt->handler = handler;

    *poll = pvt;

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("poll created: poll=%p (size=%d, timeout=%d, flags=%x)", pvt, pvt->size, pvt->timeout, pvt->flags);
#endif
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(pvt);
    }
    return status;
}

wstk_status_t wstk_poll_select_add(wstk_poll_select_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->fl_polling) {
        status = poll_socket_deferred_action(poll, socket, 1);
    } else {
        status = poll_socket_add_perform(poll, socket);
    }

    return status;
}

wstk_status_t wstk_poll_select_del(wstk_poll_select_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->fl_polling) {
        status = poll_socket_deferred_action(poll, socket, 2);
    } else {
        status = poll_socket_del_perform(poll, socket);
    }

    return status;
}

wstk_status_t wstk_poll_select_polling(wstk_poll_select_t *poll) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct timeval tv = { 0 }, *tv_ptr = NULL;
    wstk_hash_index_t *hidx = NULL;
    fd_set rdset = {0}, wrset = {0}, exset = {0};
    time_t curr_ts = 0;
    uint32_t psz = 0;
    int rc = 0, event = 0, maxfd = 0;

    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

#ifdef WSTK_POLL_DEBUG
    psz = wstk_hash_size(poll->sockets);
    WSTK_DBG_PRINT("polling-start: [poll=%p, sockets=%d]", poll, psz);
#endif

    if(wstk_hash_is_empty(poll->sockets)) {
        if(poll->flags & WSTK_POLL_FYIELD_ON_EMPTY) {
            WSTK_SCHED_YIELD(0);
        }
        return WSTK_STATUS_SUCCESS;
    }

    if(!poll->timeout) {
        tv_ptr = NULL;
    } else {
        tv.tv_sec = poll->timeout;
        tv.tv_usec = 0;
        tv_ptr = &tv;
    }

    poll->fl_polling = true;

    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    FD_ZERO(&exset);

    for(hidx = wstk_hash_first_iter(poll->sockets, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
        wstk_socket_t *sock = NULL;
        wstk_hash_this(hidx, NULL, NULL, (void *)&sock);

        if(!sock) { continue; }

        if(maxfd < sock->fd) {
            maxfd = sock->fd;
        }
        if(sock->pmask & WSTK_POLL_MREAD) {
            FD_SET(sock->fd, &rdset);
        }
        if(sock->pmask & WSTK_POLL_MWRITE) {
            FD_SET(sock->fd, &wrset);
        }
        if(sock->pmask & WSTK_POLL_MEXCEPT) {
            FD_SET(sock->fd, &exset);
        }
    }

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("polling-perform: [poll=%p, maxfd=%d]", poll, maxfd);
#endif

    rc = select(maxfd + 1, &rdset, &wrset, &exset, tv_ptr);
    if(rc < 0 || poll->fl_destroyed) {
        poll->fl_polling = false;
        return WSTK_STATUS_FALSE;
    }

    curr_ts = wstk_time_epoch_now();
    for(hidx = wstk_hash_first_iter(poll->sockets, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
        wstk_socket_t *sock = NULL;
        wstk_hash_this(hidx, NULL, NULL, (void *)&sock);

        if(!sock) { continue; }
        event = 0x0;

        if(sock->expiry && sock->expiry <= curr_ts) {
            event |= WSTK_POLL_ESEXPIRED;
        }

        if(rc > 0) {
            if(FD_ISSET(sock->fd, &rdset)) {
                event |= WSTK_POLL_EREAD;
                if(!(sock->pmask & WSTK_POLL_MLISTENER) && !(sock->pmask & WSTK_POLL_MRDLOCK)) {
                    size_t rd = 0;
                    wstk_sock_get_bytes_to_read(sock, &rd);
                    if(!rd) { event |= WSTK_POLL_ESCLOSED; }
                }
            }
            if(FD_ISSET(sock->fd, &wrset)) {
                event |= WSTK_POLL_EWRITE;
            }
            if(FD_ISSET(sock->fd, &exset)) {
                event |= WSTK_POLL_EEXCEPT;
            }
        }

        if(event) {
            poll->handler(sock, event, poll->udata);
        }
    }

    poll->fl_polling = false;

    if(!wstk_list_is_empty(poll->slist2)) {
        wstk_list_clear(poll->slist2, slist_action_perform_cb);
    }
    if(!wstk_list_is_empty(poll->slist1)) {
        wstk_list_clear(poll->slist1, slist_action_perform_cb);
    }

#ifdef WSTK_POLL_DEBUG
    psz = wstk_hash_size(poll->sockets);
    WSTK_DBG_PRINT("polling-end: [poll=%p, sockets=%d]", poll, psz);
#endif

    return WSTK_STATUS_SUCCESS;
}
