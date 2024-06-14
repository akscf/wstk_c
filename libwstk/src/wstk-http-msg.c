/**
 ** based on libre
 **
 ** (C)2024 aks
 **/
#include <wstk-http-msg.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-pl.h>
#include <wstk-regex.h>
#include <wstk-hashtable.h>

static void desctuctor__wstk_http_msg_t(void *ptr) {
    wstk_http_msg_t *msg = (wstk_http_msg_t *)ptr;

    if(!msg) { return; }

#ifdef WSTK_HTTP_MSG_DEBUG
    WSTK_DBG_PRINT("destroying http-msg: msg=%p", msg);
#endif

    if(msg->headers) {
        msg->headers = wstk_mem_deref(msg->headers);
    }

#ifdef WSTK_HTTP_MSG_DEBUG
    WSTK_DBG_PRINT("http-msg destroyed: msg=%p", msg);
#endif
}

static wstk_status_t hdr_put(wstk_http_msg_t *msg, wstk_pl_t *name, wstk_pl_t *value) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char tname[0xff] = {0};
    char *tval = NULL;

    if(!msg || !msg->headers) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(!name || !name->l || name->l >= sizeof(tname)) {
        return WSTK_STATUS_OUTOFRANGE;
    }
    if(!value || !value->l || value->l > 4096) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    memcpy((char *)tname, name->p, name->l);
    tname[name->l] = '\0';

    if(wstk_str_casecmp((char *)tname, "Content-Type") == 0) {
        const char *cend = (value->p + value->l);
        const char *cptr = wstk_strnstr(value->p, value->l, "charset=");
        if(cptr) {
            uint32_t sep_ofs = 0;
            for(sep_ofs=0; sep_ofs <= value->l; sep_ofs++) {
                if(value->p[sep_ofs] == ';') { break;}
            }
            if(sep_ofs >= value->l) {
                msg->ctype.l = value->l;
                msg->ctype.p = value->p;
            } else {
                msg->charset.l = (cend - (cptr + 8));
                msg->charset.p = (msg->charset.l ? (void *)(cptr + 8) : NULL);
                msg->ctype.l = sep_ofs;
                msg->ctype.p = value->p;
            }
        } else {
            msg->ctype.l = value->l;
            msg->ctype.p = value->p;
        }

        goto out;
    }
    if(wstk_str_casecmp((char *)tname, "Content-Length") == 0) {
        msg->clen = wstk_pl_u32(value);
        goto out;
    }

    status = wstk_pl_strdup(&tval, value);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_hash_insert_ex(msg->headers, (char *)tname, (char *)tval, true);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(tval);
    }
    return status;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new empty msg
 *
 * @param msg - a new message
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_alloc(wstk_http_msg_t **msg) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_http_msg_t *msg_local = NULL;

    if(!msg) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_mem_zalloc((void *)&msg_local, sizeof(wstk_http_msg_t), desctuctor__wstk_http_msg_t)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_hash_init(&msg_local->headers)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    *msg = msg_local;

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(msg_local);
    }
    return status;
}

/**
 * Decode mbuf with to message
 *
 * @param msg   - new message
 * @param mbuf  - http request
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_decode(wstk_http_msg_t *msg, wstk_mbuf_t *mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_pl_t b = {0}, s = {0}, e = {0}, name = {0}, value = {0};
    uint32_t ws=0, lf=0;
    bool comsep=false, quote=false;
    const char *p, *cv;
    size_t l = 0;

    if(!msg || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    msg->clen = 0;

    p = (const char *)wstk_mbuf_buf(mbuf);
    l = wstk_mbuf_left(mbuf);

    status = wstk_regex(p, l, "[\r\n]*[^\r\n]+[\r]*[\n]1", &b, &s, NULL, &e);
    if(status != WSTK_STATUS_SUCCESS) {
        if(l > mbuf->size) {
            wstk_goto_status(WSTK_STATUS_BAD_REQUEST, out);
        }
        wstk_goto_status(WSTK_STATUS_NODATA, out);
    }

    status = wstk_regex(s.p, s.l, "[a-z]+ [^? ]+[^ ]* HTTP/[0-9.]+", &msg->method, &msg->path, &msg->params, &msg->version);
    if(status != WSTK_STATUS_SUCCESS || msg->method.p != s.p) {
        wstk_goto_status(WSTK_STATUS_BAD_REQUEST, out);
    }
    if(msg->params.p && msg->params.l > 1) {
        if(*msg->params.p == '?') {
            msg->params.p++;
            msg->params.l--;
        }
    }

    l -= e.p + e.l - p;
    p = e.p + e.l;

    name.p = cv = NULL;
    name.l = ws = lf = 0;
    comsep = false;
    quote = false;

    for(; l > 0; p++, l--) {
        switch(*p) {
            case ' ':
            case '\t':
                lf = 0; /* folding */
                ++ws;
                break;
            case '\r':
                ++ws;
                break;
            case '\n':
                ++ws;
                if(!name.p) {
                    ++p; --l; /* no headers */
                    goto out;
                }
                if(!lf++) {
                    break;
                }
                ++p; --l;     /* eoh */
                /*fallthrough*/
            default:
                if(lf || (*p == ',' && comsep && !quote)) {
                    if(!name.l) {
                        wstk_goto_status(WSTK_STATUS_BAD_REQUEST, out);
                    }

                    value.p = (cv ? cv : p);
                    value.l = (cv ? p - cv - ws : 0);
                    hdr_put(msg, &name, &value);

                    if(!lf) { /* comma separated */
                        cv = NULL;
                        break;
                    }
                    if(lf > 1) { /* eoh */
                        goto out;
                    }

                    comsep = false;
                    name.p = NULL;
                    cv = NULL;
                    lf = 0;
                }
                if(!name.p) {
                    name.p = p;
                    name.l = 0;
                    ws = 0;
                }
                if(!name.l) {
                    if(*p != ':') {
                        ws = 0;
                        break;
                    }
                    name.l = MAX((int)(p - name.p - ws), 0);
                    if(!name.l) {
                        wstk_goto_status(WSTK_STATUS_BAD_REQUEST, out);
                    }
                    comsep = false;
                    break;
                }

                if(!cv) {
                    quote = false;
                    cv = p;
                }

                if(*p == '"') {
                    quote = !quote;
                }

                ws = 0;
                break;
        }
    }
    status = WSTK_STATUS_NODATA;

out:
    if(status == WSTK_STATUS_SUCCESS || status == WSTK_STATUS_NODATA) {
        mbuf->pos = (mbuf->end - l);
    }
    return status;
}

/**
 * Is the header exists in the map
 *
 * @param msg   - the message
 * @param name  - header name
 *
 * @return true/false
 **/
bool wstk_http_msg_header_exists(wstk_http_msg_t *msg, const char *name) {
    if(!msg || !name) {
        return false;
    }

    return (wstk_hash_find(msg->headers, name) ? true : false);
}

/**
 * Add header to the map
 *
 * @param msg   - the message
 * @param name  - header name
 * @param value - header value
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_header_add(wstk_http_msg_t *msg, const char *name, const char *value) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!msg || !name || !value) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(wstk_hash_find(msg->headers, name)) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
        status = wstk_hash_insert_ex(msg->headers, name, value, true);
    }
    return status;
}

/**
 * Get header value
 *
 * @param msg   - the message
 * @param name  - header name
 * @param value - header value
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_header_get(wstk_http_msg_t *msg, const char *name, const char **value) {
    void *v = NULL;

    if(!msg || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    v = wstk_hash_find(msg->headers, name);
    if(v == NULL) {
        return WSTK_STATUS_NOT_FOUND;
    }

    *value = (char *)v;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Delete header
 *
 * @param msg   - the message
 * @param name  - header name
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_header_del(wstk_http_msg_t *msg, const char *name) {
    if(!msg || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wstk_hash_delete(msg->headers, name);
    return WSTK_STATUS_SUCCESS;
}



/**
 * Dump message
 *
 * @param msg       - the message
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_http_msg_dump(wstk_http_msg_t *msg) {
    wstk_mbuf_t *tbuf = NULL;

    if(wstk_mbuf_create(&tbuf, 8192) != WSTK_STATUS_SUCCESS) {
        return WSTK_STATUS_FALSE;
    }

    wstk_mbuf_printf(tbuf, "--------------------------------------------------------\n");
    if(!msg) {
        wstk_mbuf_printf(tbuf, "msg == NULL\n");
        goto out;
    }

    wstk_mbuf_printf(tbuf, "msg-ptr.....%p\n", msg);
    wstk_mbuf_printf(tbuf, "version.....%r\n", (wstk_pl_t *) &msg->version);
    wstk_mbuf_printf(tbuf, "scheme......%r\n", (wstk_pl_t *) &msg->scheme);
    wstk_mbuf_printf(tbuf, "method......%r\n", (wstk_pl_t *) &msg->method);
    wstk_mbuf_printf(tbuf, "path........%r\n", (wstk_pl_t *) &msg->path);
    wstk_mbuf_printf(tbuf, "params......%r\n", (wstk_pl_t *) &msg->params);
    wstk_mbuf_printf(tbuf, "ctype.......%r\n", (wstk_pl_t *) &msg->ctype);
    wstk_mbuf_printf(tbuf, "charset.....%r\n", (wstk_pl_t *) &msg->charset);
    wstk_mbuf_printf(tbuf, "clen........%d\n", msg->clen);

    if(!wstk_hash_is_empty(msg->headers)) {
        wstk_hash_index_t *hidx = NULL;
        char *key = NULL, *val = NULL;
        for(hidx = wstk_hash_first_iter(msg->headers, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
            wstk_hash_this(hidx, (void *)&key, NULL, (void *)&val);
            wstk_mbuf_printf(tbuf, "HEADER: [%s] => [%s]\n", key, val);
        }
    }

out:
    wstk_mbuf_printf(tbuf, "--------------------------------------------------------\n");

    if(tbuf->pos) {
        wstk_mbuf_write_u8(tbuf, 0x0);
        log_debug("%s", tbuf->buf);
    }

    wstk_mem_deref(tbuf);
    return WSTK_STATUS_SUCCESS;
}

