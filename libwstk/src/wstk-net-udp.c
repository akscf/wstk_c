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

static wstk_status_t multicast_update(wstk_socket_t *sock, const wstk_sockaddr_t *group, bool join) {
    int af;

    if(wstk_sa_af(group, &af) != WSTK_STATUS_SUCCESS) {
        return WSTK_STATUS_FALSE;
    }

    switch(af) {
        case AF_INET: {
            struct ip_mreq mreq;
            mreq.imr_multiaddr = group->u.in.sin_addr;
            mreq.imr_interface.s_addr = 0;
            return setsockopt(sock->fd, IPPROTO_IP, (join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP), BUF_CAST &mreq, sizeof(mreq));
            break;
        }

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6: {
            struct ipv6_mreq mreq6;
            mreq6.ipv6mr_multiaddr = group->u.in6.sin6_addr;
            mreq6.ipv6mr_interface = 0;
            return setsockopt(sock->fd, IPPROTO_IPV6, (join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP), BUF_CAST &mreq6, sizeof(mreq6));
            break;
        }
#endif
    }

    return WSTK_STATUS_UNSUPPORTED;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_udp_connect(wstk_socket_t **sock, const wstk_sockaddr_t *peer) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock_local = NULL;

    if(!sock || !peer) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_sock_alloc(&sock_local, AF_UNSPEC, IPPROTO_UDP, 0)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_sa_af(peer, &sock_local->type)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((sock_local->fd = socket(sock_local->type, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("connect: socket() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(connect(sock_local->fd, &peer->u.sa, peer->len) != 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("connect: connect() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_sock_set_blocking(sock_local, false)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("connect: set_blocking() err=%i", sock_local->err);
#endif
        goto out;
    }

    sock_local->fl_connected = true;
    *sock = sock_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(sock_local);
    }
    return status;
}

wstk_status_t wstk_udp_listen(wstk_socket_t **sock, const wstk_sockaddr_t *local) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock_local = NULL;
    int err;

    if(!sock || !local) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_sock_alloc(&sock_local, AF_UNSPEC, IPPROTO_UDP, 0)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_sa_af(local, &sock_local->type)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((sock_local->fd = socket(sock_local->type, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("listen: socket() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_sock_set_reuse(sock_local, true)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("listen: set_reuse() err=%i", sock_local->err);
#endif
        goto out;
    }

    if(bind(sock_local->fd, &local->u.sa, local->len) < 0) {
        sock_local->err = WSTK_SOCK_ERROR;
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("listen: bind() err=%i", sock_local->err);
#endif
        wstk_goto_status(WSTK_STATUS_BIND_FAIL, out);
    }

    if((status = wstk_sock_set_blocking(sock_local, false)) != WSTK_STATUS_SUCCESS) {
#ifdef WSTK_UDP_LOG_ERRORS
        log_error("listen: set_blocking() err=%i", sock_local->err);
#endif
        goto out;
    }

    *sock = sock_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(sock_local);
    }
    return status;
}

/**
 * Send data
 * function modifies: mbuf->pos (will be ponted at a new position)
 *
 * @param sock      - the socket
 * @param dsk       - the destination address or null for client mode
 * @param mbuf      - buffer to send
 *
 * @return succes or error
 **/
wstk_status_t wstk_udp_send(wstk_socket_t *sock, const wstk_sockaddr_t *dst, wstk_mbuf_t *mbuf) {
    ssize_t rc;

    if(!mbuf || !sock || sock->proto != IPPROTO_UDP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    /* client */
    if(sock->fl_connected) {
        rc = send(sock->fd, (char *)wstk_mbuf_buf(mbuf), wstk_mbuf_left(mbuf), 0);
        if(rc < 0) {
            sock->err = rc;
            return WSTK_STATUS_FALSE;
        }

        wstk_mbuf_advance(mbuf, rc);
        return WSTK_STATUS_SUCCESS;
    }

    /* server */
    if(!dst) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    switch(sock->type) {
        case AF_INET:
            rc = sendto(sock->fd, (char *)wstk_mbuf_buf(mbuf), wstk_mbuf_left(mbuf), 0, (struct sockaddr *)&dst->u.in, dst->len);
            break;
#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            rc = sendto(sock->fd, (char *)wstk_mbuf_buf(mbuf), wstk_mbuf_left(mbuf), 0, (struct sockaddr *)&dst->u.in6, dst->len);
            break;
#endif
        default:
            return WSTK_STATUS_UNSUPPORTED;
    }

    if(rc < 0) {
        sock->err = WSTK_SOCK_ERROR;
        return WSTK_STATUS_FALSE;
    }

    wstk_mbuf_advance(mbuf, rc);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Read data from the socket
 * The buffer will be growing, during new data feeds, if you want to have a fixed size, set mbuf->pos=0 before a call
 * function modifies: mbuf->pos and mbuf->end
 *
 * @param sock      - the socket
 * @param src       - peer address
 * @param mbuf      - buffer for data
 * @param timeout   - 0 or time in seconds to wait for
 *
 * @return succes or error
 **/
wstk_status_t wstk_udp_recv(wstk_socket_t *sock, wstk_sockaddr_t *src, wstk_mbuf_t *mbuf, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *ptr = NULL;
    size_t rsz = 0;
    int rc = 0, sel_rc = 0;

    if(!src || !mbuf || !sock || sock->proto != IPPROTO_UDP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    status = wstk_sock_get_bytes_to_read(sock, &rsz);
    if(status != WSTK_STATUS_SUCCESS) { return status; }
    if(!rsz) { return WSTK_STATUS_NODATA; }

    if(wstk_mbuf_space(mbuf) < rsz) {
        status = wstk_mbuf_resize(mbuf, (mbuf->size + MAX(rsz, 4096)) );
        if(status != WSTK_STATUS_SUCCESS) { return status; }
    }

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

    ptr = (char *)wstk_mbuf_buf(mbuf);
    src->len = sizeof(src->u);

    rc = recvfrom(sock->fd, ptr, rsz, 0, &src->u.sa, &src->len);
    if(rc < 0) {
        sock->err = WSTK_SOCK_ERROR;
        if(sock->err == EAGAIN || sock->err == EWOULDBLOCK) {
            status = WSTK_STATUS_NODATA;
            goto out;
        }
        status = WSTK_STATUS_FALSE;
    } else {
        if(rc > 0) {
            mbuf->pos += rc;
            mbuf->end = mbuf->pos;
        } else {
            status = WSTK_STATUS_NODATA;
        }
    }
out:
    return status;
}

/**
 * Join to the multicat group
 *
 * @param sock      - the socket
 * @param group     - the group
 *
 * @return succes or error
 **/
wstk_status_t wstk_udp_multicast_join(wstk_socket_t *sock, const wstk_sockaddr_t *group) {
    if(!group || !sock || sock->proto != IPPROTO_UDP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    return multicast_update(sock, group, true);
}

/**
 * Leave the group
 *
 * @param sock      - the socket
 * @param group     - the group
 *
 * @return succes or error
 **/
wstk_status_t wstk_udp_multicast_leave(wstk_socket_t *sock, const wstk_sockaddr_t *group) {
    if(!group || !sock || sock->proto != IPPROTO_UDP) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(sock->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    return multicast_update(sock, group, false);
}

