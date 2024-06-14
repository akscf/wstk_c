/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-uri.h>
#include <wstk-log.h>
#include <wstk-net.h>
#include <wstk-fmt.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-pl.h>
#include <wstk-regex.h>

static wstk_status_t uri_decode_hostport(const wstk_pl_t *hostport, wstk_pl_t *host, wstk_pl_t *port) {
    if(!hostport || !host || !port) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    /* Try IPv6 first */
    if(wstk_regex(hostport->p, hostport->l, "\\[[0-9a-f:]+\\][:]*[0-9]*", host, NULL, port) == WSTK_STATUS_SUCCESS) {
        return WSTK_STATUS_SUCCESS;
    }

    /* Then non-IPv6 host */
    return wstk_regex(hostport->p, hostport->l, "[^:]+[:]*[0-9]*", host, NULL, port);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_uri_encode_mbuf(wstk_uri_t *uri, wstk_mbuf_t *mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    bool has_userpass = false;

    if(!uri || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!wstk_pl_isset(&uri->scheme) || !wstk_pl_isset(&uri->host)) {
        return WSTK_STATUS_INVALID_VALUE;
    }

    status = wstk_mbuf_printf(mbuf, "%r://", &uri->scheme);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if(wstk_pl_isset(&uri->user)) {
        status = wstk_mbuf_printf(mbuf, "%r", &uri->user);
        if(status != WSTK_STATUS_SUCCESS) { goto out; }

        if(wstk_pl_isset(&uri->password)) {
            status = wstk_mbuf_printf(mbuf, ":%r", &uri->password);
            if(status != WSTK_STATUS_SUCCESS) { goto out; }
        }

        status = wstk_mbuf_printf(mbuf, "@");
        if(status != WSTK_STATUS_SUCCESS) { goto out; }

        has_userpass = true;
    }

    switch(uri->af) {
#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            status = wstk_mbuf_printf(mbuf, "[%r]", &uri->host);
            break;
#endif
        default:
            status = wstk_mbuf_printf(mbuf, "%r", &uri->host);
    }

    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(uri->port) {
        status = wstk_mbuf_printf(mbuf, ":%u", uri->port);
        if(status != WSTK_STATUS_SUCCESS) { goto out; }
    }

    status = wstk_mbuf_printf(mbuf, "/");
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

out:
    return status;
}

wstk_status_t wstk_uri_encode(wstk_uri_t *uri, char **str) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *mbuf = NULL;
    char *str_local = NULL;

    status = wstk_mbuf_create(&mbuf, 4096);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_uri_encode_mbuf(uri, mbuf);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    wstk_mbuf_set_pos(mbuf, 0);
    status = wstk_mbuf_strdup(mbuf, &str_local, wstk_mbuf_end(mbuf));
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    *str = str_local;
out:
    wstk_mem_deref(mbuf);
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(str_local);
    }
    return status;
}


wstk_status_t wstk_uri_decode(wstk_uri_t *uri, const char *str, size_t str_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_sockaddr_t addr = { 0 };
    wstk_pl_t port = WSTK_PL_INIT;
    wstk_pl_t hostport = WSTK_PL_INIT;
    wstk_pl_t params = WSTK_PL_INIT;
    wstk_pl_t headers = WSTK_PL_INIT;

    if(!uri || !str) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    memset(uri, 0, sizeof(wstk_uri_t));
    if(wstk_regex(str, str_len, "[^:]+://[^@:]*[:]*[^@]*@[^;? ]+[^?]*[^]*", &uri->scheme, &uri->user, NULL, &uri->password, &hostport, &params, &headers) == WSTK_STATUS_SUCCESS) {
        if(uri_decode_hostport(&hostport, &uri->host, &port) == WSTK_STATUS_SUCCESS) {
            goto out;
        }
    }

    memset(uri, 0, sizeof(wstk_uri_t));
    if(wstk_regex(str, str_len, "[^:]+://[^;? ]+[^?]*[^]*", &uri->scheme, &hostport, &params, &headers) == WSTK_STATUS_SUCCESS) {
        if(uri_decode_hostport(&hostport, &uri->host, &port) == WSTK_STATUS_SUCCESS) {
            goto out;
        }
    }

    return WSTK_STATUS_FALSE;

out:
    if(wstk_sa_set_pl(&addr, &uri->host, 0) == WSTK_STATUS_SUCCESS) {
        wstk_sa_af(&addr, &uri->af);
    } else {
        uri->af = AF_UNSPEC;
    }

    if(wstk_pl_isset(&port)) {
        uri->port = (uint16_t)wstk_pl_u32(&port);
    }

    return WSTK_STATUS_SUCCESS;
}
