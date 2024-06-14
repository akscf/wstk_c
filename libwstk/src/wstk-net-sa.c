/**
 ** based on libre
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-pl.h>

/** IPv4 Link-local test */
#define IN_IS_ADDR_LINKLOCAL(a) (((a) & htonl(0xffff0000)) == htonl (0xa9fe0000))

static void desctuctor__wstk_sockaddr_t(void *ptr) {
    wstk_sockaddr_t *sa = (wstk_sockaddr_t *)ptr;

}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_sa_inet_pton(const char *addr, wstk_sockaddr_t *sa) {
    if(!addr) {
        return  WSTK_STATUS_INVALID_PARAM;
    }

    if(wstk_net_pton(AF_INET, addr, &sa->u.in.sin_addr) > 0) {
        sa->u.in.sin_family = AF_INET;
    }
#ifdef WSTK_NET_HAVE_INET6
    else if (wstk_net_pton(AF_INET6, addr, &sa->u.in6.sin6_addr) > 0) {
        if (IN6_IS_ADDR_V4MAPPED(&sa->u.in6.sin6_addr)) {
            const uint8_t *a = &sa->u.in6.sin6_addr.s6_addr[12];
            sa->u.in.sin_family = AF_INET;
            memcpy(&sa->u.in.sin_addr.s_addr, a, 4);
        } else {
            sa->u.in6.sin6_family = AF_INET6;
        }
    }
#endif
    else {
        return WSTK_STATUS_UNSUPPORTED;
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_sa_inet_ntop(const wstk_sockaddr_t *sa, char *buf, int size) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!sa || !buf || !size) {
        return  WSTK_STATUS_INVALID_PARAM;
    }

    switch(sa->u.sa.sa_family) {
        case AF_INET:
            status = wstk_net_ntop(AF_INET, &sa->u.in.sin_addr, buf, size) > 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE;
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            status = wstk_net_ntop(AF_INET6, &sa->u.in6.sin6_addr, buf, size) > 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE;
            break;
#endif
        default:
            status = WSTK_STATUS_UNSUPPORTED;
    }

    return status;
}


wstk_status_t wstk_sa_init(wstk_sockaddr_t *sa, int af) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    memset(sa, 0, sizeof(wstk_sockaddr_t));
    sa->u.sa.sa_family = (af == AF_UNSPEC ? AF_INET : af);
    sa->len = sizeof(sa->u);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_sa_set_pl(wstk_sockaddr_t *sa, const wstk_pl_t *addr, uint16_t port) {
    char buf[64] = { 0 };

    wstk_sa_init(sa, 0);

    wstk_pl_strcpy(addr, buf, sizeof(buf));
    return wstk_sa_set_str(sa, buf, port);
}

wstk_status_t wstk_sa_set_str(wstk_sockaddr_t *sa, const char *addr, uint16_t port) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!sa || !addr) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wstk_sa_init(sa, 0);

    status = wstk_sa_inet_pton(addr, sa);
    if(status != WSTK_STATUS_SUCCESS) {
        return status;
    }

    switch(sa->u.sa.sa_family) {
        case AF_INET:
            sa->u.in.sin_port = htons(port);
            sa->len = sizeof(struct sockaddr_in);
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            sa->u.in6.sin6_port = htons(port);
            sa->len = sizeof(struct sockaddr_in6);
            break;
#endif
        default:
            return WSTK_STATUS_UNSUPPORTED;
    }

    return WSTK_STATUS_SUCCESS;
}


wstk_status_t wstk_sa_set_in(wstk_sockaddr_t *sa, uint32_t addr, uint16_t port) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    sa->u.in.sin_family = AF_INET;
    sa->u.in.sin_addr.s_addr = htonl(addr);
    sa->u.in.sin_port = htons(port);
    sa->len = sizeof(struct sockaddr_in);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_sa_set_in6(wstk_sockaddr_t *sa, const uint8_t *addr, uint16_t port) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

#ifdef WSTK_NET_HAVE_INET6
    sa->u.in6.sin6_family = AF_INET6;
    memcpy(&sa->u.in6.sin6_addr, addr, 16);
    sa->u.in6.sin6_port = htons(port);
    sa->len = sizeof(struct sockaddr_in6);
    return WSTK_STATUS_SUCCESS;
#else
    (void)addr;
    (void)port;
    return WSTK_STATUS_UNSUPPORTED;
#endif
}

wstk_status_t wstk_sa_set_sa(wstk_sockaddr_t *sa, const struct sockaddr *s) {
    if(!sa ||!s) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    switch (s->sa_family) {
        case AF_INET:
            memcpy(&sa->u.in, s, sizeof(struct sockaddr_in));
            sa->len = sizeof(struct sockaddr_in);
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            memcpy(&sa->u.in6, s, sizeof(struct sockaddr_in6));
            sa->len = sizeof(struct sockaddr_in6);
            break;
#endif

        default:
            return WSTK_STATUS_UNSUPPORTED;
    }

    sa->u.sa.sa_family = s->sa_family;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_sa_set_port(wstk_sockaddr_t *sa, uint16_t port) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    switch (sa->u.sa.sa_family) {
        case AF_INET:
            sa->u.in.sin_port = htons(port);
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            sa->u.in6.sin6_port = htons(port);
            break;
#endif
        default:
            return WSTK_STATUS_UNSUPPORTED;
    }

    return WSTK_STATUS_SUCCESS;
}


/**
 * Set a socket address from a string of type "address:port"
 * IPv6 addresses must be encapsulated in square brackets.
 * Example strings:
 *   1.2.3.4:1234
 *   [::1]:1234
 *   [::]:5060
 */
wstk_status_t wstk_sa_decode(wstk_sockaddr_t *sa, const char *str, size_t len) {
    wstk_pl_t addr, port, pl;
    const char *c;

    if(!sa || !str || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    pl.p = str;
    pl.l = len;

    if('[' == str[0] && (c = wstk_pl_strchr(&pl, ']'))) {
        addr.p = str + 1;
        addr.l = c - str - 1;
        ++c;
    } else if (NULL != (c = wstk_pl_strchr(&pl, ':'))) {
        addr.p = str;
        addr.l = c - str;
    } else {
        return WSTK_STATUS_INVALID_VALUE;
    }

    if(len < (size_t)(c - str + 2)) {
        return WSTK_STATUS_INVALID_VALUE;
    }

    if(':' != *c) {
        return WSTK_STATUS_INVALID_VALUE;
    }

    port.p = ++c;
    port.l = len + str - c;

    return wstk_sa_set_pl(sa, &addr, wstk_pl_u32(&port));
}

/**
 * Get the Address Family of a Socket Address
 * af - address family
 */
wstk_status_t wstk_sa_af(const wstk_sockaddr_t *sa, int *af) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *af = sa->u.sa.sa_family;
    return WSTK_STATUS_SUCCESS;
}


/**
 * Get the IPv4-address of a Socket Address
 * in - IPv4 address in host order
 */
wstk_status_t wstk_sa_in(const wstk_sockaddr_t *sa, uint32_t *in) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *in = ntohl(sa->u.in.sin_addr.s_addr);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get the IPv6-address of a Socket Address
 * addr - contains the IPv6-address
 */
wstk_status_t wstk_sa_in6(const wstk_sockaddr_t *sa, uint8_t *addr) {
    if(!sa || !addr) {
        return WSTK_STATUS_INVALID_PARAM;
    }

#ifdef WSTK_NET_HAVE_INET6
    memcpy(addr, &sa->u.in6.sin6_addr, 16);
    return WSTK_STATUS_SUCCESS;
#else
    return WSTK_STATUS_UNSUPPORTED;
#endif
}

/**
 * Get the port number from a Socket Address
 * port -  Port number  in host order
 */
wstk_status_t wstk_sa_port(const wstk_sockaddr_t *sa, uint16_t *port) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    switch (sa->u.sa.sa_family) {
        case AF_INET:
            *port = ntohs(sa->u.in.sin_port);
            return WSTK_STATUS_SUCCESS;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            *port = ntohs(sa->u.in6.sin6_port);
            return WSTK_STATUS_SUCCESS;
#endif
    }

    return WSTK_STATUS_FALSE;
}

/**
 * Check if a Socket Address is set
 *
 * @param sa   Socket Address
 * @param flag Flags specifying which fields to check
 *
 * @return true if set, false if not set
 */
bool wstk_sa_is_set(const wstk_sockaddr_t *sa, int flag) {
    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    switch (sa->u.sa.sa_family) {
        case AF_INET:
            if(flag & WSTK_SA_ADDR) {
                if(INADDR_ANY == sa->u.in.sin_addr.s_addr) {
                    return false;
                }
            }
            if(flag & WSTK_SA_PORT) {
                if(!sa->u.in.sin_port) {
                    return false;
                }
            }
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            if(flag & WSTK_SA_ADDR) {
                if(IN6_IS_ADDR_UNSPECIFIED(&sa->u.in6.sin6_addr)) {
                    return false;
                }
            }
            if(flag & WSTK_SA_PORT) {
                if(!sa->u.in6.sin6_port) {
                    return false;
                }
            }
            break;
#endif

        default:
                return false;
    }

    return true;
}

wstk_status_t wstk_sa_cpy(wstk_sockaddr_t *dst, const wstk_sockaddr_t *src) {
    if(!dst || !src) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    memcpy(dst, src, sizeof(wstk_sockaddr_t));
    return WSTK_STATUS_SUCCESS;
}

/**
 * Compare two Socket Address objects
 *
 * @param l    Socket Address number one
 * @param r    Socket Address number two
 * @param flag Flags specifying which fields to use
 *
 * @return true if match, false if no match
 */
bool wstk_sa_cmp(const wstk_sockaddr_t *l, const wstk_sockaddr_t *r, int flag) {
    if(!l || !r) {
        return false;
    }
    if(l == r) {
        return true;
    }

    if(l->u.sa.sa_family != r->u.sa.sa_family) {
        return false;
    }

    switch(l->u.sa.sa_family) {
        case AF_INET:
            if(flag & WSTK_SA_ADDR) {
                if(l->u.in.sin_addr.s_addr != r->u.in.sin_addr.s_addr) {
                    return false;
                }
            }
            if(flag & WSTK_SA_PORT) {
                if(l->u.in.sin_port != r->u.in.sin_port) {
                    return false;
                }
            }
            break;

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            if(flag & WSTK_SA_ADDR) {
                if(memcmp(&l->u.in6.sin6_addr, &r->u.in6.sin6_addr, 16)) {
                    return false;
                }
            }
            if(flag & WSTK_SA_PORT) {
                if(l->u.in6.sin6_port != r->u.in6.sin6_port) {
                    return false;
                }
            }
            break;
#endif
        default:
            return false;
        }

        return true;
}

/**
 * Check if socket address is a link-local address
 *
 * @param sa Socket address
 * @return true if link-local address, otherwise false
 */
bool wstk_sa_is_linklocal(const wstk_sockaddr_t *sa) {
    int fa = 0;

    if(!sa) {
        return false;
    }
    if(wstk_sa_af(sa, &fa) != WSTK_STATUS_SUCCESS) {
        return false;
    }

    switch(fa) {
        case AF_INET:
            return IN_IS_ADDR_LINKLOCAL(sa->u.in.sin_addr.s_addr);

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            return IN6_IS_ADDR_LINKLOCAL(&sa->u.in6.sin6_addr);
#endif
    }

    return false;
}

/**
 * Check if socket address is a loopback address
 *
 * @param sa Socket address
 * @return true if loopback address, otherwise false
 */
bool wstk_sa_is_loopback(const wstk_sockaddr_t *sa) {
    int fa = 0;

    if(!sa) {
        return false;
    }
    if(wstk_sa_af(sa, &fa) != WSTK_STATUS_SUCCESS) {
        return false;
    }

    switch(fa) {
        case AF_INET:
            return INADDR_LOOPBACK == ntohl(sa->u.in.sin_addr.s_addr);

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            return IN6_IS_ADDR_LOOPBACK(&sa->u.in6.sin6_addr);
#endif
    }

    return false;
}

/**
 * Check if socket address is any/unspecified address
 *
 * @param sa Socket address
 * @return true if any address, otherwise false
 */
bool wstk_sa_is_any(const wstk_sockaddr_t *sa) {
    int fa = 0;

    if(!sa) {
        return false;
    }
    if(wstk_sa_af(sa, &fa) != WSTK_STATUS_SUCCESS) {
        return false;
    }

    switch(fa) {
        case AF_INET:
            return INADDR_ANY == ntohl(sa->u.in.sin_addr.s_addr);

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            return IN6_IS_ADDR_UNSPECIFIED(&sa->u.in6.sin6_addr);
#endif
    }

    return false;
}

wstk_status_t wstk_sa_hash(const wstk_sockaddr_t *sa, uint32_t *hash) {
    uint32_t v = 0;

    if(!sa) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    switch(sa->u.sa.sa_family) {
        case AF_INET:
            v += ntohl(sa->u.in.sin_addr.s_addr);
            v += ntohs(sa->u.in.sin_port);
            v += sa->u.sa.sa_family;
        break;
#ifdef HAVE_INET6
        case AF_INET6:
            uint32_t *a = (uint32_t *)&sa->u.in6.sin6_addr;
            v += a[0] ^ a[1] ^ a[2] ^ a[3];
            v += ntohs(sa->u.in6.sin6_port);
            v += sa->u.sa.sa_family;
        break;
#endif
        default:
            return WSTK_STATUS_UNSUPPORTED;
    }

    *hash = v;

    return WSTK_STATUS_SUCCESS;;
}

