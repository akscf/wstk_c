/**
 **
 ** (C)2024 aks
 **/
#include <wstk-poll.h>
#include <wstk-log.h>
#include <wstk-mem.h>

struct wstk_poll_s {
    void                    *pvt;
    wstk_polling_method_e   method;
    bool                    fl_destroyed;
};

static void destructor__wstk_poll_t(void *data) {
    wstk_poll_t *poll = (wstk_poll_t *)data;

    if(!poll || poll->fl_destroyed) {
        return;
    }
    poll->fl_destroyed = true;

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("destroying poll: poll=%p (pvt=%p)", poll, poll->pvt);
#endif

    if(poll->pvt) {
        poll->pvt = wstk_mem_deref(poll->pvt);
    }

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("poll destroyed: poll=%p", poll);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new poll
 *
 * @param poll     - the poll
 * @param method   - using method
 * @param size     - pool size
 * @param timeout  - timeout in seconds
 * @param flags    - poll flags
 * @param handler  - hadler
 * @param udata    - udata
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_create(wstk_poll_t **poll, wstk_polling_method_e method, uint32_t size, uint32_t timeout, uint32_t flags, wstk_poll_handler_t handler, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_poll_auto_conf_t aconf = { 0 };
    wstk_poll_t *poll_local = NULL;

    if(!poll || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&poll_local, sizeof(wstk_poll_t), destructor__wstk_poll_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    aconf = wstk_poll_auto_conf(method, size);
    poll_local->method = aconf.method;

    if(poll_local->method == WSTK_POLL_SELECT) {
        status = wstk_poll_select_create((void *)&poll_local->pvt, size, timeout, flags, handler, udata);
    } else if(poll_local->method == WSTK_POLL_EPOLL) {
        status = wstk_poll_epoll_create((void *)&poll_local->pvt, size, timeout, flags, handler, udata);
    } else if(poll_local->method == WSTK_POLL_KQUEUE) {
        status = wstk_poll_kqueue_create((void *)&poll_local->pvt, size, timeout, flags, handler, udata);
    } else {
        wstk_goto_status(WSTK_STATUS_UNSUPPORTED, out);
    }

#ifdef WSTK_POLL_DEBUG
    WSTK_DBG_PRINT("poll created: poll=%p (method=%d, pvt=%p)", poll_local, poll_local->method, poll_local->pvt);
#endif

    *poll = poll_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(poll_local);
    }
    return status;
}

/**
 * Add socket to the pool
 *
 * @param poll     - the poll
 * @param socket   - socket
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_add(wstk_poll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sc = NULL;

    if(!poll || !socket || socket->fl_destroyed || !socket->pmask) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        status = wstk_poll_select_add((wstk_poll_select_t *)poll->pvt, socket);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        status = wstk_poll_epoll_add((wstk_poll_epoll_t *)poll->pvt, socket);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        status = wstk_poll_kqueue_add((wstk_poll_kqueue_t *)poll->pvt, socket);
    } else {
        status = WSTK_STATUS_UNSUPPORTED;
    }

    if(status == WSTK_STATUS_SUCCESS) {
#ifdef WSTK_POLL_DEBUG
        WSTK_DBG_PRINT("socket added: poll=%p (socket=%p, fd=%d, mask=0x%x)", poll, socket, socket->fd, socket->pmask);
#endif
    }

    return status;
}

/**
 * Delete socket from the pool
 *
 * @param poll     - a new poll
 * @param socket   - socket
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_del(wstk_poll_t *poll, wstk_socket_t *socket) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll || !socket || socket->fl_destroyed) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        status = wstk_poll_select_del((wstk_poll_select_t *)poll->pvt, socket);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        status = wstk_poll_epoll_del((wstk_poll_epoll_t *)poll->pvt, socket);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        status = wstk_poll_kqueue_del((wstk_poll_kqueue_t *)poll->pvt, socket);
    } else {
        status = WSTK_STATUS_UNSUPPORTED;
    }

    if(status == WSTK_STATUS_SUCCESS) {
#ifdef WSTK_POLL_DEBUG
        WSTK_DBG_PRINT("socket deleted: poll=%p (socket=%p, fd=%d, mask=0x%x)", poll, socket, socket->fd, socket->pmask);
#endif
    }
    return status;
}

/**
 * Polling
 *
 * @param poll     - the poll
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_polling(wstk_poll_t *poll) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        status = wstk_poll_select_polling((wstk_poll_select_t *)poll->pvt);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        status = wstk_poll_epoll_polling((wstk_poll_epoll_t *)poll->pvt);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        status = wstk_poll_kqueue_polling((wstk_poll_kqueue_t *)poll->pvt);
    } else {
        status = WSTK_STATUS_UNSUPPORTED;
    }

    return status;
}

/**
 * Is it contains anything
 *
 * @param poll     - the poll
 *
 * @return true/false
 **/
bool wstk_poll_is_empty(wstk_poll_t *poll) {
    if(!poll || poll->fl_destroyed) {
        return false;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        return wstk_poll_select_is_empty((wstk_poll_select_t *)poll->pvt);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        return wstk_poll_epoll_is_empty((wstk_poll_epoll_t *)poll->pvt);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        return wstk_poll_kqueue_is_empty((wstk_poll_kqueue_t *)poll->pvt);
    }

    return false;
}

/**
 * Get a left space in the pool
 *
 * @param poll    - the poll
 * @param space   - space
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_space(wstk_poll_t *poll, uint32_t *space) {
    if(!poll || !space) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        return wstk_poll_select_space((wstk_poll_select_t *)poll->pvt, space);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        return wstk_poll_epoll_space((wstk_poll_epoll_t *)poll->pvt, space);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        return wstk_poll_kqueue_space((wstk_poll_kqueue_t *)poll->pvt, space);
    }

    return WSTK_STATUS_UNSUPPORTED;
}

/**
 * Get poll size
 *
 * @param poll   - the poll
 * @param size   - the size
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_size(wstk_poll_t *poll, uint32_t *size) {
    if(!poll || !size) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_SELECT) {
        return wstk_poll_select_size((wstk_poll_select_t *)poll->pvt, size);
    } else if(poll->method == WSTK_POLL_EPOLL) {
        return wstk_poll_epoll_size((wstk_poll_epoll_t *)poll->pvt, size);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        return wstk_poll_kqueue_size((wstk_poll_kqueue_t *)poll->pvt, size);
    }

    return WSTK_STATUS_UNSUPPORTED;
}

/**
 * Get polling method
 *
 * @param poll     - the poll
 * @param method   - method
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_poll_method(wstk_poll_t *poll, wstk_polling_method_e *method) {
    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *method = poll->method;
    return WSTK_STATUS_SUCCESS;
}

/**
 * propose a prefered poll for requests
 *
 * @param method    - requests method
 * @param size      - requests size
 *
 * @return wstk_poll_auto_conf_t
 **/
wstk_poll_auto_conf_t wstk_poll_auto_conf(wstk_polling_method_e method, uint32_t size) {
    wstk_poll_auto_conf_t conf = {0};

    conf.size = size;
    conf.method = method;

    if(method == WSTK_POLL_AUTO) {
        if(wstk_poll_epoll_is_supported()) {
            conf.method = WSTK_POLL_EPOLL;
        }
        if(wstk_poll_kqueue_is_supported()) {
            conf.method = WSTK_POLL_KQUEUE;
        }

        if(method == WSTK_POLL_AUTO) {
            if(wstk_poll_select_is_supported()) {
                conf.method = WSTK_POLL_SELECT;
            }
        }
    }

    if(conf.method == WSTK_POLL_SELECT) {
        if(conf.size > wstk_poll_select_max_size()) {
           conf.size =  wstk_poll_select_max_size();
        }
    }

    return conf;
}

/**
 * Interrupt
 *
 * @param poll     - the poll
 *
 * @return success
 **/
wstk_status_t wstk_poll_interrupt(wstk_poll_t *poll) {
    if(!poll) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(poll->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(poll->method == WSTK_POLL_EPOLL) {
        return wstk_poll_epoll_interrupt((wstk_poll_epoll_t *)poll->pvt);
    } else if(poll->method == WSTK_POLL_KQUEUE) {
        return wstk_poll_kqueue_interrupt((wstk_poll_kqueue_t *)poll->pvt);
    }

    return WSTK_STATUS_SUCCESS;
}
