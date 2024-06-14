/**
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-time.h>

#ifdef WSTK_OS_WIN
 #define close closesocket
#endif

static void desctuctor__wstk_socket_t(void *ptr) {
    wstk_socket_t *sock = (wstk_socket_t *)ptr;

    if(!sock || sock->fl_destroyed) {
        return;
    }
    sock->fl_destroyed = true;

#ifdef WSTK_SOCK_DEBUG
    WSTK_DBG_PRINT("destroying socket: sock=%p (fd=%d, type=%d, proto=%d, ssl-ctx=%p, udate=%p, udata_destroy=%d)", sock, sock->fd, sock->type, sock->proto, sock->ssl_ctx, sock->udata, sock->fl_adestroy_udata);
#endif

    if(sock->fd) {
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
    }

    if(sock->ssl_ctx) {
        sock->ssl_ctx = wstk_mem_deref(sock->ssl_ctx);
    }

    if(sock->udata && sock->fl_adestroy_udata) {
        sock->udata = wstk_mem_deref(sock->udata);
    }

#ifdef WSTK_SOCK_DEBUG
    WSTK_DBG_PRINT("socket destroyed: sock=%p", sock);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new socket
 *
 * @param sock   - new socket
 * @param type   - socket type
 * @param proto  - socket proto
 * @param fd     - descriptor or 0
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_alloc(wstk_socket_t **sock, int type, int proto, int fd) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock_local = NULL;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&sock_local, sizeof(wstk_socket_t), desctuctor__wstk_socket_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    sock_local->fd = fd;
    sock_local->type = type;
    sock_local->proto = proto;

#ifdef WSTK_SOCK_DEBUG
    WSTK_DBG_PRINT("socket created: sock=%p (type=%d, proto=%d, fd=%d)", sock_local, sock_local->type, sock_local->proto, sock_local->fd);
#endif

    *sock = sock_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(sock_local);
        *sock = NULL;
    }
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get socket local address
 *
 * @param sock  - the socket
 * @param local - the result
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_get_local(wstk_socket_t *sock, wstk_sockaddr_t *local) {
    if(!sock || !local) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_sa_init(local, AF_UNSPEC);
    if(getsockname(sock->fd, &local->u.sa, &local->len) != 0) {
        local->err = errno;
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Get socket peer address
 *
 * @param sock  - the socket
 * @param peer  - the result
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_get_peer(wstk_socket_t *sock, wstk_sockaddr_t *peer) {
    if(!sock || !peer) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_sa_init(peer, AF_UNSPEC);
    if(getpeername(sock->fd, &peer->u.sa, &peer->len) != 0) {
        peer->err = errno;
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Get the number of bytes that are immediately available for reading.
 *
 * @param sock  - the socket
 * @param bytes - the result
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_get_bytes_to_read(wstk_socket_t *sock, size_t *bytes) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    if(!sock || !bytes) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

#ifdef WSTK_OS_WIN
    u_long sz = 0;
    ioctlsocket(sock->fd, FIONREAD, &sz);
    *bytes = sz;
#else
    int sz = 0;
    ioctl(sock->fd, FIONREAD, (void *)&sz);
    *bytes = sz;
#endif

    return status;
}

/**
 * Set/Clear O_NONBLOCK flag
 *
 * @param sock          - the socket
 * @param reuse         - true/fale
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_blocking(wstk_socket_t *sock, bool blocking) {
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

#ifdef WSTK_OS_WIN
    unsigned long noblock = !blocking;
    if(ioctlsocket(sock->fd, FIONBIO, &noblock) != 0) {
        sock->err = WSAGetLastError();
        return WSTK_STATUS_FALSE;
    }
    return WSTK_STATUS_SUCCESS;
#else
    int flags;
    int err = 0;

    flags = fcntl(sock->fd, F_GETFL);
    if(flags < 0) {
        sock->err = errno;
        return WSTK_STATUS_FALSE;
    }

    if(blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    if(fcntl(sock->fd, F_SETFL, flags) < 0) {
        sock->err = errno;
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
#endif
}

/**
 * Set/Clear SO_REUSEADDR flag
 *
 * @param sock          - the socket
 * @param reuse         - true/fale
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_reuse(wstk_socket_t *sock, bool reuse) {
    int r = reuse;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

#ifdef SO_REUSEADDR
    if(setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &r, sizeof(r)) < 0) {
        sock->err = errno;
        return WSTK_STATUS_FALSE;
    }
#endif

#ifdef SO_REUSEPORT
    if(setsockopt(sock->fd, SOL_SOCKET, SO_REUSEPORT, (void *) &r, sizeof(r)) < 0) {
        sock->err = errno;
        return WSTK_STATUS_FALSE;
    }
#endif

#if !defined(SO_REUSEADDR) && !defined(SO_REUSEPORT)
        return WSTK_STATUS_NOT_IMPL;
#else
        return WSTK_STATUS_SUCCESS;
#endif
}

/**
 * Set/Clear TCP_NODELAY flag
 *
 * @param sock          - the socket
 * @param val           - true/fale
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_nodelay(wstk_socket_t *sock, bool val) {
    int r = val;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(setsockopt(sock->fd, SOL_SOCKET, TCP_NODELAY, (void *) &r, sizeof(r)) < 0) {
        sock->err = errno;
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Set udata
 *
 * @param sock          - the socket
 * @param udata         - some data
 * @param auto_destroy  - destroy when socket closed
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_udata(wstk_socket_t *sock, void *udata, bool auto_destroy) {
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    sock->udata = udata;
    sock->fl_adestroy_udata = (udata ? auto_destroy : false);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Set poll mask
 *
 * @param sock          - the socket
 * @param mask          - mask
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_pmask(wstk_socket_t *sock, uint32_t mask) {
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    sock->pmask = mask;
    return WSTK_STATUS_SUCCESS;
}


/**
 * Set timeout
 *
 * @param sock          - the socket
 * @param mask          - mask
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_set_expiry(wstk_socket_t *sock, uint32_t timeout) {
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    sock->expiry = (timeout ? wstk_time_epoch_now() + timeout : 0);
    return WSTK_STATUS_SUCCESS;
}

/**
 * close descriptor
 *
 * @param sock          - the socket
 *
 * @return succes or error
 **/
wstk_status_t wstk_sock_close_fd(wstk_socket_t *sock) {
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(sock->fd) {
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
        sock->fd = 0;
    }

    return WSTK_STATUS_SUCCESS;
}
