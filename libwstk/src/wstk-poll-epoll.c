/**
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-poll.h>
#include <wstk-mem.h>
#include <wstk-sleep.h>
#include <wstk-mutex.h>
#include <wstk-thread.h>
#include <wstk-hashtable.h>
#include <wstk-list.h>
#include <wstk-time.h>

#ifdef WSTK_HAVE_EPOLL
#include <sys/epoll.h>
#endif

struct wstk_poll_epoll_s {
#ifdef WSTK_HAVE_EPOLL
    struct epoll_event      *events;
#endif
    wstk_mutex_t            *mutex;
    wstk_inthash_t          *sockets;
    wstk_list_t             *slist1;
    wstk_list_t             *slist2;
    void                    *udata;
    wstk_poll_handler_t     handler;
    int                     epfd;
    uint32_t                flags;
    uint32_t                size;
    uint32_t                timeout;
    bool                    fl_polling;
    bool                    fl_destroyed;
};

typedef struct {
    wstk_poll_epoll_t   *poll;
    wstk_socket_t       *socket;
    int                 act;
} slist_entry_t;


#ifdef WSTK_HAVE_EPOLL
static void destructor__wstk_poll_epoll_t(void *data) {
    wstk_poll_epoll_t *poll = (wstk_poll_epoll_t *)data;

    if(!poll || poll->fl_destroyed) {
        return;
    }
    poll->fl_destroyed = true;

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("destroying poll: poll=%p ", poll);
#endif

    if(poll->fl_polling) {
        while(poll->fl_polling) {
            WSTK_SCHED_YIELD(0);
        }
    }

    wstk_mutex_lock(poll->mutex);
    if(poll->epfd >= 0) {
        close(poll->epfd);
        poll->epfd = -1;
    }
    wstk_mutex_unlock(poll->mutex);

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
    poll->events = wstk_mem_deref(poll->events);
    poll->mutex = wstk_mem_deref(poll->mutex);

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("poll destoyed: poll=%p ", poll);
#endif
}

static wstk_status_t poll_socket_add_perform(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct epoll_event event = {0};
    int err=0, fd = socket->fd;

    if(wstk_hash_size(poll->sockets) >= poll->size) {
        return WSTK_STATUS_NOSPACE;
    }

    if(wstk_core_inthash_find(poll->sockets, socket->fd)) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
        status = wstk_inthash_insert(poll->sockets, socket->fd, socket);
        if(status == WSTK_STATUS_SUCCESS) {
            event.data.ptr = socket;

            if(socket->pmask & WSTK_POLL_MREAD)  {
                event.events |= EPOLLIN;
            }
            if(socket->pmask & WSTK_POLL_MWRITE) {
                event.events |= EPOLLOUT;
            }
            if(socket->pmask & WSTK_POLL_MEXCEPT) {
                event.events |= EPOLLERR;
            }

            if(epoll_ctl(poll->epfd, EPOLL_CTL_ADD, fd, &event) < 0) {
                err = errno;
                log_error("Unable to add socket: err=%d (sock=%p, fd=%d)", err, socket, fd);
                status = WSTK_STATUS_FALSE;
            }
        }
        if(status != WSTK_STATUS_SUCCESS) {
            wstk_core_inthash_delete(poll->sockets, fd);
        }
    }

    return status;
}

static wstk_status_t poll_socket_del_perform(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    int fd = socket->fd;

    if(wstk_core_inthash_delete(poll->sockets, socket->fd)) {
        epoll_ctl(poll->epfd, EPOLL_CTL_DEL, fd, NULL);
    }

    return status;
}

static wstk_status_t poll_deferred_action(wstk_poll_epoll_t *poll, wstk_socket_t *socket, int act) {
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
                wstk_mem_ref(socket);
            }
        }
    }

    return status;
}

static void slist_action_perform_cb(uint32_t pos, void *data, void *udata) {
    slist_entry_t *entry = (slist_entry_t *)data;
    wstk_status_t st = 0;

    if(entry) {
        if(entry->act == 1) {
            st = poll_socket_add_perform(entry->poll, entry->socket);
            if(st != WSTK_STATUS_SUCCESS) {
                log_error("Unable to add socket to the pool (poll=%p, sock=%p, status=%d)", entry->poll, entry->socket, (int)st);
                wstk_mem_deref(entry->socket);
            }
        } else if(entry->act == 2) {
            st = poll_socket_del_perform(entry->poll, entry->socket);
            wstk_mem_deref(entry->socket);
        }
        wstk_mem_deref(entry);
    }
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool wstk_poll_epoll_is_supported() {
    return true;
}

bool wstk_poll_epoll_is_empty(wstk_poll_epoll_t *poll) {
    if(!poll || poll->fl_destroyed) {
        return false;
    }
    return wstk_hash_is_empty(poll->sockets);
}

wstk_status_t wstk_poll_epoll_interrupt(wstk_poll_epoll_t *poll) {
    wstk_socket_t *sock = NULL;
    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    //
    // todo
    //

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_poll_epoll_space(wstk_poll_epoll_t *poll, uint32_t *space) {
    if(!poll || !space) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *space = (poll->size - wstk_hash_size(poll->sockets));
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_poll_epoll_size(wstk_poll_epoll_t *poll, uint32_t *size) {
    if(!poll || !size) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

   *size = poll->size;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_poll_epoll_create(wstk_poll_epoll_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_poll_epoll_t *pvt = NULL;

    status = wstk_mem_zalloc((void *)&pvt, sizeof(wstk_poll_epoll_t), destructor__wstk_poll_epoll_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&pvt->mutex)) != WSTK_STATUS_SUCCESS) {
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

    pvt->size = (size ? size : 1024);
    pvt->timeout = (timeout ? timeout : 60);
    pvt->handler = handler;
    pvt->flags = flags;
    pvt->udata = udata;

    pvt->epfd = -1;
    pvt->epfd = epoll_create(pvt->size);
    if(pvt->epfd < 0) {
        log_error("epoll: err=%d (errno=%d)", pvt->epfd, errno);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    status = wstk_mem_zalloc((void *)&pvt->events, (pvt->size * sizeof(*pvt->events)), NULL);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

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

wstk_status_t wstk_poll_epoll_add(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll || poll->fl_destroyed) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->fl_polling) {
        status = poll_deferred_action(poll, socket, 1);
    } else {
        status = poll_socket_add_perform(poll, socket);
    }

    return status;
}

wstk_status_t wstk_poll_epoll_del(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->fl_polling) {
        status = poll_deferred_action(poll, socket, 2);
    } else {
        status = poll_socket_del_perform(poll, socket);
    }

    return status;
}

wstk_status_t wstk_poll_epoll_polling(wstk_poll_epoll_t *poll) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_hash_index_t *hidx = NULL;
    time_t curr_ts = 0;
    uint32_t psz=0, fds=0;
    int rc = 0, event = 0;

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

    poll->fl_polling = true;
    fds = wstk_hash_size(poll->sockets);

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("polling-perform: [poll=%p, fds=%d]", poll, fds);
#endif

    rc = epoll_wait(poll->epfd, poll->events, fds, (poll->timeout ? (poll->timeout * 1000): -1));
    if(rc < 0 || poll->fl_destroyed) {
        poll->fl_polling = false;
        return WSTK_STATUS_FALSE;
    }

    if(rc > 0) {
        for(int i=0; i < rc; i++) {
            struct epoll_event *eev = &poll->events[i];
            wstk_socket_t *sock = eev->data.ptr;

            event = 0x0;
            if(sock) {
                if(eev->events & EPOLLIN) {
                    event |= WSTK_POLL_EREAD;
                    if(!(sock->pmask & WSTK_POLL_MLISTENER) && !(sock->pmask & WSTK_POLL_MRDLOCK)) {
                        size_t rd = 0;
                        wstk_sock_get_bytes_to_read(sock, &rd);
                        if(!rd) { event |= WSTK_POLL_ESCLOSED; }
                    }
                }
                if(eev->events & EPOLLOUT) {
                    event |= WSTK_POLL_EWRITE;
                }
                if(eev->events & (EPOLLERR|EPOLLHUP)) {
                    event |= WSTK_POLL_EEXCEPT;
                }

                if(event) {
                    poll->handler(sock, event, poll->udata);
                }
            }
        }
    }

    /* who expired */
    curr_ts = wstk_time_epoch_now();
    for(hidx = wstk_hash_first_iter(poll->sockets, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
        wstk_socket_t *sock = NULL;
        wstk_hash_this(hidx, NULL, NULL, (void *)&sock);

        if(!sock) { continue; }

        if(sock->expiry && sock->expiry <= curr_ts) {
            poll->handler(sock, WSTK_POLL_ESEXPIRED, poll->udata);
        }
    }

    poll->fl_polling = false;

    if(!wstk_list_is_empty(poll->slist2)) {
        wstk_list_clear(poll->slist2, slist_action_perform_cb, NULL);
    }
    if(!wstk_list_is_empty(poll->slist1)) {
        wstk_list_clear(poll->slist1, slist_action_perform_cb, NULL);
    }

#ifdef WSTK_POLL_DEBUG
    psz = wstk_hash_size(poll->sockets);
    WSTK_DBG_PRINT("polling-end: [poll=%p, sockets=%d]", poll, psz);
#endif

    return status;
}

#else /* WSTK_HAVE_EPOLL */
bool wstk_poll_epoll_is_supported() {
    return false;
}
bool wstk_poll_epoll_is_empty(wstk_poll_epoll_t *poll) {
    return true;
}
wstk_status_t wstk_poll_epoll_interrupt(wstk_poll_epoll_t *poll){
    return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_size(wstk_poll_epoll_t *poll, uint32_t *size) {
    return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_space(wstk_poll_epoll_t *poll, uint32_t *space) {
   return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_create(wstk_poll_epoll_t **poll, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata) {
    return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_add(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_del(wstk_poll_epoll_t *poll, wstk_socket_t *socket) {
    return WSTK_STATUS_UNSUPPORTED;
}
wstk_status_t wstk_poll_epoll_polling(wstk_poll_epoll_t *poll) {
    return WSTK_STATUS_UNSUPPORTED;
}

#endif
