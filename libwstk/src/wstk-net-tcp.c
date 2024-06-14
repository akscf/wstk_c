/**
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-pl.h>

#ifdef WSTK_OS_WIN
 #define close closesocket
 #define BUF_CAST (char *)
#else
 #define BUF_CAST
#endif

static int tcp_vprintf_handler(const char *p, size_t size, void *arg) {
    wstk_socket_t *sock = (wstk_socket_t *)arg;
    ssize_t rc = 0;

    if(!sock || !p) {
        return WSTK_STATUS_FALSE;
    }

    rc = send(sock->fd, p, size, 0);
    if(rc < 0) {
        sock->err = WSTK_SOCK_ERROR;
        return WSTK_STATUS_FALSE;
    }
    if(rc == 0) {
        return WSTK_STATUS_CONN_DISCON;
    }

    return (rc >= size ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
}

static inline bool err_is_conn_fail(int err) {
#ifdef WSTK_OS_WIN
    if(err == WSAECONNABORTED || err == WSAECONNRESET || err == WSAEHOSTUNREACH || err == WSAENETDOWN) {
        return true;
    }
#else
    if(err == ECONNREFUSED || err == ENETUNREACH || err == EHOSTUNREACH || err == EHOSTDOWN) {
        return true;
    }
#endif
    return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 /**
 * Create a new socket and connect to the target
 *
 * @param sock      - a new socket
 * @param peer      - adress to connect
 * @param timeout   - 0 or time in seconds to wait for
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_connect(wstk_socket_t **sock, const wstk_sockaddr_t *peer, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock_local = NULL;

    if(!sock || !peer) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_sock_alloc(&sock_local, AF_UNSPEC, IPPROTO_TCP, 0)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_sa_af(peer, &sock_local->type)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((sock_local->fd = socket(sock_local->type, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("connect: socket() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_sock_set_blocking(sock_local, false)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("connect: set_blocking() err=%i", sock_local->err);
#endif
        goto out;
    }

    if(connect(sock_local->fd, &peer->u.sa, peer->len) != 0) {
        sock_local->err = WSTK_SOCK_ERROR;
        if(err_is_conn_fail(sock_local->err)) {
            wstk_goto_status(WSTK_STATUS_CONN_FAIL, out);
        }
        if(sock_local->err == EINPROGRESS) {
            wstk_goto_status(WSTK_STATUS_SUCCESS, wait);
        }
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("connect: connect() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

wait:
    if(timeout > 0) {
        struct timeval tv = {0};
        fd_set fdset;

        FD_ZERO(&fdset);
        FD_SET(sock_local->fd, &fdset);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        if(select(sock_local->fd + 1, NULL, &fdset, NULL, &tv) > 0) {
            socklen_t len = sizeof(sock_local->err);
            getsockopt(sock_local->fd, SOL_SOCKET, SO_ERROR, BUF_CAST &sock_local->err, &len);
            if(sock_local->err == 0) {
                status = WSTK_STATUS_SUCCESS;
            } else {
                status = WSTK_STATUS_CONN_FAIL;
            }
        } else {
            sock_local->err = WSTK_SOCK_ERROR;
            status = WSTK_STATUS_TIMEOUT;
        }
    }

    if(status == WSTK_STATUS_SUCCESS) {
        sock_local->err = 0;
        sock_local->fl_connected = true;
        *sock = sock_local;
    }

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(sock_local);
        *sock = NULL;
    }
    return status;
}

/**
 * Create a new socket and start listener
 *
 * @param sock   - a new socket
 * @param local  - adress for listen to
 * @param nconn  - max queued connections for listen
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_listen(wstk_socket_t **sock, const wstk_sockaddr_t *local, uint32_t nconn) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock_local = NULL;

    if((status = wstk_sock_alloc(&sock_local, AF_UNSPEC, IPPROTO_TCP, 0)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_sa_af(local, &sock_local->type)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((sock_local->fd = socket(sock_local->type, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("listen: socket() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_sock_set_reuse(sock_local, true)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("listen: set_reuse() err=%i", sock_local->err);
#endif
        goto out;
    }

    if((status = wstk_sock_set_blocking(sock_local, false)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("listen: set_blocking() err=%i", sock_local->err);
#endif
        goto out;
    }

    if(bind(sock_local->fd, &local->u.sa, local->len) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("listen: bind() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(listen(sock_local->fd, nconn) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_TCP_LOG_ERRORS
        log_error("listen: listen() err=%i", sock_local->err);
#endif
        goto out;
    }

    *sock = sock_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(sock_local);
        *sock = NULL;
    }
    return status;
}

/**
 * Accept new connections on the listener socket
 *
 * @param sock      - listener socket
 * @param cli_sock  - a new client socket
 * @param timeout   - 0 or time in seconds to wait for
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_accept(wstk_socket_t *sock, wstk_socket_t **cli_sock, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *cslocal = NULL;
    int fd = 0;

    if(!sock || sock->proto != IPPROTO_TCP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(timeout > 0) {
        struct timeval tv = {0};
        fd_set fdset;
        int err = 0;

        FD_ZERO(&fdset);
        FD_SET(sock->fd, &fdset);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        err = select(sock->fd + 1, &fdset, NULL, NULL, &tv);
        if(err < 0) {
            return WSTK_STATUS_FALSE;
        }
        if(err == 0) {
            return WSTK_STATUS_NODATA;
        }
    }

    if((fd = accept(sock->fd, NULL, NULL)) < 0) {
        sock->err = WSTK_SOCK_ERROR;
        if(sock->err == EWOULDBLOCK || sock->err == EAGAIN) {
            sock->err = 0;
            return WSTK_STATUS_NODATA;
        }
        return WSTK_STATUS_FALSE;
    }

    if((status = wstk_sock_alloc(&cslocal, sock->type, sock->proto, fd)) != WSTK_STATUS_SUCCESS) {
        close(fd);
        goto out;
    }
    if((status = wstk_sock_set_blocking(cslocal, false)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    *cli_sock = cslocal;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(cslocal);
        cli_sock = NULL;
    }
    return status;
}

/**
 * Write mbuf to the socket
 * function modifies: mbuf->pos (will be ponted at a new position)
 *
 * @param sock      - socket
 * @param mbuf      - buffer
 * @param timeout   - 0 or time in seconds to wait for
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_write(wstk_socket_t *sock, wstk_mbuf_t *mbuf, uint32_t timeout) {
    ssize_t rc = 0;
    size_t wsz = 0;
    char *ptr = NULL;

    if(!mbuf || !sock || sock->proto != IPPROTO_TCP) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(timeout > 0) {
        struct timeval tv = {0};
        fd_set fdset;
        int err = 0;

        FD_ZERO(&fdset);
        FD_SET(sock->fd, &fdset);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        err = select(sock->fd + 1, NULL, &fdset, NULL, &tv);
        if(err < 0) {
            return WSTK_STATUS_FALSE;
        }
        if(err == 0) {
            return WSTK_STATUS_NODATA;
        }
    }

    ptr = (char *)wstk_mbuf_buf(mbuf);
    wsz = wstk_mbuf_left(mbuf);

    rc = send(sock->fd, ptr, wsz, 0);
    if(rc < 0) {
        sock->err = WSTK_SOCK_ERROR;
        return WSTK_STATUS_FALSE;
    }
    if(rc == 0) {
        return WSTK_STATUS_CONN_DISCON;
    }

    wstk_mbuf_advance(mbuf, rc);
out:
    return WSTK_STATUS_SUCCESS;
}

/**
 * Read data from the socket into mbuf
 * The buffer will be growing, during new data feeds, if you want to have a fixed size, set mbuf->pos=0 before a call
 * function modifies: mbuf->pos and mbuf->end
 *
 * @param sock      - socket
 * @param mbuf      - buffer
 * @param timeout   - 0 or time in seconds to wait for
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_read(wstk_socket_t *sock, wstk_mbuf_t *mbuf, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *ptr = NULL;
    size_t rsz = 0;
    int rc = 0, sel_rc = 0;

    if(!mbuf || !sock || sock->proto != IPPROTO_TCP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    status = wstk_sock_get_bytes_to_read(sock, &rsz);
    if(status != WSTK_STATUS_SUCCESS) { return status; }
    if(!rsz && !timeout) { return WSTK_STATUS_NODATA; }

    if(wstk_mbuf_space(mbuf) < rsz) {
        status = wstk_mbuf_resize(mbuf, (mbuf->size + MAX(rsz, 4096)) );
        if(status != WSTK_STATUS_SUCCESS) { return status; }
    }
    ptr = (char *)wstk_mbuf_buf(mbuf);

    soselect:
    if(timeout > 0) {
        struct timeval tv = {0};
        fd_set fdset;

        FD_ZERO(&fdset);
        FD_SET(sock->fd, &fdset);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        sel_rc = select(sock->fd + 1, &fdset, NULL, NULL, &tv);
        if(sel_rc < 0) {
            return WSTK_STATUS_FALSE;
        }
        if(sel_rc == 0) {
            return WSTK_STATUS_NODATA;
        }
    }

    rc = recv(sock->fd, ptr, rsz, 0);
    if(rc < 0) {
        sock->err = WSTK_SOCK_ERROR;
        if(sock->err == EAGAIN || sock->err == EWOULDBLOCK) {
            wstk_goto_status(WSTK_STATUS_NODATA, out);
        }
        if(sock->err == EPIPE) {
            wstk_goto_status(WSTK_STATUS_CONN_DISCON, out);
        }
        status = WSTK_STATUS_FALSE;
    } else {
        if(rc > 0) {
            mbuf->pos += rc;
            mbuf->end = mbuf->pos;
            sock->rderr = 0;
        } else {
            sock->err = WSTK_SOCK_ERROR;
            if(sock->err == EAGAIN || sock->err == EWOULDBLOCK) {
                status = WSTK_STATUS_CONN_DISCON;
            } else {
                if(sel_rc && !rc) { sock->rderr++; }
                if(sock->rderr > 10) { status = WSTK_STATUS_CONN_DISCON; }
                else { status = WSTK_STATUS_NODATA; }
            }
        }
    }
out:
    return status;
}

/**
 * Write helper
 *
 * @param sock  - socket
 * @param fmt   - formatted string
 * @param ap    - arguments
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_vprintf(wstk_socket_t *sock, const char *fmt, va_list ap) {
    return wstk_vhprintf(fmt, ap, tcp_vprintf_handler, sock);
}

/**
 * Write helper
 *
 * @param sock  - socket
 * @param fmt   - formatted string
 *
 * @return succes or error
 **/
wstk_status_t wstk_tcp_printf(wstk_socket_t *sock, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    va_list ap;

    if(!sock || sock->proto != IPPROTO_TCP) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    va_start(ap, fmt);
    status = wstk_vhprintf(fmt, ap, tcp_vprintf_handler, sock);
    va_end(ap);

    return status;
}

