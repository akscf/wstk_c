/**
 ** WebSocket servlet
 **
 ** (C)2024 aks
 **/
#include <wstk-servlet-websock.h>
#include <wstk-log.h>
#include <wstk-websock.h>
#include <wstk-httpd.h>
#include <wstk-pl.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-mbuf.h>
#include <wstk-thread.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-time.h>
#include <wstk-base64.h>
#include <wstk-sha1.h>
#include <wstk-hashtable.h>

#define WEBSOCK_CONTENT_MAX_LENGTH  1048576  // 1Mb
#define WEBSOCK_ATTR__WS_CONN       "ws-conn-sys"

static const uint8_t magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *WS_BAD_REQUEST_MSG = "Bad request (websock)";

struct wstk_servlet_websock_s {
    wstk_mutex_t                        *mutex;
    wstk_inthash_t                      *sockets;   // websockets (con-id > wstk_http_conn_t)
    uint32_t                            refs;
    bool                                fl_destroyed;
    //
    wstk_servlet_websock_on_close_t     hnd_on_close;
    wstk_servlet_websock_on_accept_t    hnd_on_accept;
    wstk_servlet_websock_on_message_t   hnd_on_message;
};

/* helper struct */
typedef struct {
    wstk_http_conn_t        *http_conn;
    wstk_tcp_srv_conn_t     *tcp_conn;
    wstk_servlet_websock_t  *servlet;
    wstk_httpd_sec_ctx_t    *sec_ctx;
    uint32_t                conn_id;
    bool                    fl_destroyed;
    bool                    fl_registered;
    bool                    fl_chnd_called;
} websock_tcp_conn_ws_attr_t;

static wstk_status_t ws_reg(wstk_servlet_websock_t *servlet, wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *sec_ctx);
static void ws_unreg(wstk_http_conn_t *conn);

// ---------------------------------------------------------------------------------------------------------------------
static void desctuctor__websock_tcp_conn_ws_attr_t(void *ptr) {
    websock_tcp_conn_ws_attr_t *attr = (websock_tcp_conn_ws_attr_t *)ptr;
    wstk_servlet_websock_t *servlet = (attr ? attr->servlet : NULL);

    if(!attr || !servlet || attr->fl_destroyed) {
        return;
    }

#ifdef WSTK_SERVLET_WEBSOCK_DEBUG
    WSTK_DBG_PRINT("unregister websocket: conn=%p (conn-id=%d, tcp-conn=%p, sec-ctx=%p, servlet=%p)", attr->http_conn, attr->conn_id, attr->tcp_conn, attr->sec_ctx, servlet);
#endif

    if(attr->fl_registered) {
        wstk_mutex_lock(servlet->mutex);
        wstk_core_inthash_delete(servlet->sockets, attr->conn_id);
        wstk_mutex_unlock(servlet->mutex);
    }

    /* perform onClose handler */
    if(!attr->fl_chnd_called) {
        attr->fl_chnd_called = true;
        if(servlet->hnd_on_close) {
            servlet->hnd_on_close(attr->http_conn, attr->sec_ctx);
        }
    }

    if(attr->sec_ctx) {
        wstk_httpd_sec_ctx_clean(attr->sec_ctx);
        attr->sec_ctx = wstk_mem_deref(attr->sec_ctx);
    }

#ifdef WSTK_SERVLET_WEBSOCK_DEBUG
    WSTK_DBG_PRINT("websocket unregistered: conn=%p", attr->http_conn);
#endif
}

static void desctuctor__wstk_servlet_websock_t(void *ptr) {
    wstk_servlet_websock_t *servlet = (wstk_servlet_websock_t *)ptr;
    bool floop = true;

    if(!servlet || servlet->fl_destroyed) {
        return;
    }
    servlet->fl_destroyed = true;

#ifdef WSTK_SERVLET_WEBSOCK_DEBUG
    WSTK_DBG_PRINT("destroying servlet: servlet=%p (refs=%d)", servlet, servlet->refs);
#endif

    if(servlet->mutex) {
        while(floop) {
            wstk_mutex_lock(servlet->mutex);
            floop = (servlet->refs > 0);
            wstk_mutex_unlock(servlet->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(servlet->refs) {
        log_warn("Lost references (refs=%d)", servlet->refs);
    }

    if(servlet->sockets) {
        wstk_mutex_lock(servlet->mutex);
        servlet->sockets = wstk_mem_deref(servlet->sockets);
        wstk_mutex_unlock(servlet->mutex);
    }

    servlet->mutex = wstk_mem_deref(servlet->mutex);

#ifdef WSTK_SERVLET_WEBSOCK_DEBUG
    WSTK_DBG_PRINT("servlet destroyed: servlet=%p", servlet);
#endif
}

static void servlet_perform_handler(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    wstk_servlet_websock_t *servlet = (wstk_servlet_websock_t *)udata;
    wstk_status_t status = 0;
    const char *hdr_val = NULL, *ws_key = NULL;
    wstk_mbuf_t *buffer = NULL;
    wstk_websock_hdr_t ws_hdr = { 0 };
    wstk_httpd_sec_ctx_t sec_ctx = {0};
    wstk_servlet_websock_conn_t websock_conn = {0};
    websock_tcp_conn_ws_attr_t *ws_conn_attr = NULL;
    int ws_hits = 0;

    if(!servlet) {
        log_error("oops! (servlet == null)");
        wstk_httpd_ereply(conn, 500, NULL);
        return;
    }

    /* accept new connection */
    if(!conn->websock) {
        if(!msg) {
            wstk_httpd_ereply(conn, 400, WS_BAD_REQUEST_MSG);
            return;
        }
        if(wstk_http_msg_header_get(msg, "Connection", &hdr_val) == WSTK_STATUS_SUCCESS || wstk_strstr(hdr_val, "Upgrade")) {
            ws_hits++;
        }
        if(wstk_http_msg_header_get(msg, "Upgrade", &hdr_val) == WSTK_STATUS_SUCCESS || wstk_str_equal(hdr_val, "websocket", false)) {
            ws_hits++;
        }
        if(wstk_http_msg_header_get(msg, "Sec-WebSocket-Version", &hdr_val) == WSTK_STATUS_SUCCESS && wstk_str_equal(hdr_val, "13", false)) {
            ws_hits++;
        }
        if(ws_hits < 3) {
            wstk_httpd_ereply(conn, 400, WS_BAD_REQUEST_MSG);
            return;
        }

        if(wstk_http_msg_header_get(msg, "Sec-WebSocket-Key", &ws_key) == WSTK_STATUS_SUCCESS && ws_key) {
            char digest[WSTK_SHA1_DIGEST_SIZE] = {0};
            wstk_sha1_t sha = {0};
            char *akey = NULL;

            wstk_sha1_init(&sha);
            wstk_sha1_update(&sha, (uint8_t *)ws_key, strlen(ws_key));
            wstk_sha1_update(&sha, magic, sizeof(magic) - 1);
            wstk_sha1_final(&sha, (uint8_t *)digest);

            if(wstk_base64_encode_str(digest, sizeof(digest), &akey, NULL) == WSTK_STATUS_SUCCESS) {
                status = wstk_httpd_reply(conn, 101, "Switching Protocols",
                                                            "Upgrade: websocket\r\n"
                                                            "Connection: Upgrade\r\n"
                                                            "Sec-WebSocket-Accept: %s\r\n"
                                                            "\r\n",
                                                            akey
                                        );
                conn->websock = (status == WSTK_STATUS_SUCCESS ? true : false);
            }
            wstk_mem_deref(akey);
        }

        if(conn->websock) {
            bool sock_allow = true;

            /* authenticate */
            if(servlet->hnd_on_accept) {
                wstk_httpd_autheticate(conn, msg, &sec_ctx);
                sock_allow = servlet->hnd_on_accept(conn, &sec_ctx);
            }
            if(sock_allow) {
                if((status = ws_reg(servlet, conn, &sec_ctx)) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to register websock (conn=%p, status=%d)", conn, (int)status);
                    conn->websock = false;
                    wstk_httpd_sec_ctx_clean(&sec_ctx);
                    wstk_httpd_ereply(conn, 500, NULL);
                }
            } else {
                conn->websock = false;
                wstk_httpd_sec_ctx_clean(&sec_ctx);
                wstk_httpd_ereply(conn, 401, "Connection unauthorized (websock)");
            }
        } else {
            wstk_httpd_ereply(conn, 400, WS_BAD_REQUEST_MSG);
        }
        return;
    }

    /* websockk messages */
    if(!conn->buffer) {
        log_error("Unable to get connection buffer");
        conn->websock = false;
        wstk_httpd_ereply(conn, 500, NULL);
        goto out;
    }

    if(wstk_tcp_srv_conn_attr_get(conn->tcp_conn, WEBSOCK_ATTR__WS_CONN, (void *)&ws_conn_attr) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to get ws-attr");
        conn->websock = false;
        wstk_httpd_ereply(conn, 500, NULL);
        goto out;
    }

    wstk_mbuf_set_pos(conn->buffer, 0);
    status = wstk_websock_decode(&ws_hdr, conn->buffer);
    if(status == WSTK_STATUS_NODATA) { goto out; }
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unable to decode websock msg");
        wstk_httpd_ereply(conn, 400, WS_BAD_REQUEST_MSG);
        ws_unreg(conn);
        goto out;
    }

    if(ws_hdr.opcode == WEBSOCK_PING) {
        wstk_servlet_websock_send(conn, WEBSOCK_PONG, "%b", wstk_mbuf_buf(conn->buffer), wstk_mbuf_left(conn->buffer));
        goto out;
    }
    if(ws_hdr.opcode == WEBSOCK_PONG) {
        goto out;
    }

    if(ws_hdr.opcode == WEBSOCK_CLOSE) {
        /* NOTE: the handler will be performed from the destructor */
        wstk_servlet_websock_send(conn, WEBSOCK_CLOSE, "%b", wstk_mbuf_buf(conn->buffer), wstk_mbuf_left(conn->buffer));
        wstk_tcp_srv_conn_close(conn->tcp_conn);
        ws_unreg(conn);
        goto out;
    }

    if(ws_hdr.len > WEBSOCK_CONTENT_MAX_LENGTH) {
        log_error("Message is to big (%d > %d)", (uint32_t)ws_hdr.len, WEBSOCK_CONTENT_MAX_LENGTH);
        ws_unreg(conn);
        wstk_httpd_ereply(conn, 400, WS_BAD_REQUEST_MSG);
        goto out;
    }

    if(ws_hdr.len > wstk_mbuf_left(conn->buffer)) {
        if(wstk_mbuf_alloc(&buffer, ws_hdr.len) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate buffer");
            ws_unreg(conn);
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }
        /* read first part and more */
        wstk_mbuf_write_mem(buffer, wstk_mbuf_buf(conn->buffer), wstk_mbuf_left(conn->buffer));

        if(wstk_httpd_conn_rdlock(conn, true) == WSTK_STATUS_SUCCESS) {
            while(true) {
                status = wstk_httpd_read(conn, buffer, WSTK_RD_TIMEOUT(ws_hdr.len));
                if(servlet->fl_destroyed || buffer->pos >= ws_hdr.len)  {
                    break;
                }
                if(status == WSTK_STATUS_CONN_DISCON || status == WSTK_STATUS_CONN_EXPIRE)  {
                    break;
                }
            }
            wstk_httpd_conn_rdlock(conn, false);
        } else {
            status = WSTK_STATUS_LOCK_FAIL;
            log_error("Unable to lock connection (rdlock)");
        }

        if(!WSTK_RW_ACCEPTABLE(status)) {
            log_error("Unable to read the whole message (status=%d)", (int)status);
            ws_unreg(conn);
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }

        if(servlet->hnd_on_message) {
            websock_conn.header = &ws_hdr;
            websock_conn.sec_ctx = ws_conn_attr->sec_ctx;
            websock_conn.http_conn = conn;

            wstk_mbuf_set_pos(buffer, 0);
            servlet->hnd_on_message(&websock_conn, buffer);
        }

    } else {
        if(servlet->hnd_on_message) {
            websock_conn.header = &ws_hdr;
            websock_conn.sec_ctx = ws_conn_attr->sec_ctx;
            websock_conn.http_conn = conn;

            servlet->hnd_on_message(&websock_conn, conn->buffer);
        }
    }

out:
    if(buffer) {
        wstk_mem_deref(buffer);
    }
}

/* helper to catch tcp_conn destroy */
static wstk_status_t ws_reg(wstk_servlet_websock_t *servlet, wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *sec_ctx) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    websock_tcp_conn_ws_attr_t *attr = NULL;

    status = wstk_mem_zalloc((void *)&attr, sizeof(websock_tcp_conn_ws_attr_t), desctuctor__websock_tcp_conn_ws_attr_t);
    if(status != WSTK_STATUS_SUCCESS) {
        return status;
    }

    /* cloning ctx */
    if((status = wstk_httpd_sec_ctx_clone(&attr->sec_ctx, sec_ctx)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    attr->servlet = servlet;
    attr->conn_id = conn->conn_id;
    attr->tcp_conn = conn->tcp_conn;
    attr->http_conn = conn;

    status = wstk_tcp_srv_conn_attr_add(conn->tcp_conn, WEBSOCK_ATTR__WS_CONN, attr, true);
    if(status == WSTK_STATUS_SUCCESS) {
        attr->fl_registered = true;

        wstk_mutex_lock(servlet->mutex);
        status = wstk_inthash_insert(servlet->sockets, attr->conn_id, conn);
        wstk_mutex_unlock(servlet->mutex);
    }

out:
    if(status != WSTK_STATUS_SUCCESS) {
        if(attr && attr->fl_registered) {
            wstk_tcp_srv_conn_attr_del(conn->tcp_conn, WEBSOCK_ATTR__WS_CONN);
        } else {
            wstk_mem_deref(attr);
        }
        conn->websock = false;
    } else {
#ifdef WSTK_SERVLET_WEBSOCK_DEBUG
        WSTK_DBG_PRINT("websocket added: conn=%p (conn-id=%d, tcp-conn=%p, servlet=%p)", attr->http_conn, attr->conn_id, attr->tcp_conn, servlet);
#endif
    }
    return status;
}

/* clear websock flag and delete from registry */
static void ws_unreg(wstk_http_conn_t *conn) {
    conn->websock = false;
    wstk_tcp_srv_conn_attr_del(conn->tcp_conn, WEBSOCK_ATTR__WS_CONN);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create and register websocket servlet
 *
 * @param srv       - the server
 * @param path      - servlet path
 * @param servlet   - a new servlet instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_register_servlet_websock(wstk_httpd_t *srv, char *path, wstk_servlet_websock_t **servlet) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_servlet_websock_t *servlet_local = NULL;

    if(!servlet || !srv || !path) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&servlet_local, sizeof(wstk_servlet_websock_t), desctuctor__wstk_servlet_websock_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&servlet_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_inthash_init(&servlet_local->sockets)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_httpd_register_servlet(srv, path, servlet_perform_handler, servlet_local, true);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    *servlet = servlet_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(servlet_local);
    }
    return status;
}

/**
 * Register handler: onAccept
 * called when going a new connection
 *
 * @param servlet   - the servlet
 * @param handler   - handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_websock_set_on_accept(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_accept_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->hnd_on_accept = handler;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Register handler: onClose
 * called when connection closing/distroying
 *
 * @param servlet   - the servlet
 * @param handler   - handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_websock_set_on_close(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_close_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->hnd_on_close = handler;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Register handler: onMessage
 * called when appear new messages
 *
 * @param servlet   - websock servlet instance
 * @param handler   - handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_websock_set_on_message(wstk_servlet_websock_t *servlet, wstk_servlet_websock_on_message_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->hnd_on_message = handler;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Send message
 * send message to the client by connection, usually uses from handlers
 *
 * @param conn      - the connection
 * @param opcode    - ws opcode
 * @param fmt       - formatted msg
 * @param ...       - params
 *
 * @return success or error
 **/
wstk_status_t wstk_servlet_websock_send(wstk_http_conn_t *conn, websock_opcode_e opcode, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock = NULL;
    va_list ap;

    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(wstk_tcp_srv_conn_is_closed(conn->tcp_conn)) {
        return WSTK_STATUS_CONN_DISCON;
    }

    if((status = wstk_tcp_srv_conn_socket(conn->tcp_conn, &sock)) != WSTK_STATUS_SUCCESS) {
        return status;
    }
    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    va_start(ap, fmt);
    status = wstk_websock_vsend(sock, opcode, 0, true, fmt, ap);
    va_end(ap);

    return status;
}

/**
 * Send message
 * send message to the client by connection-id, useful from outside code
 *
 * @param servlet   - the servlet
 * @param conn_id   - the connection id
 * @param opcode    - ws opcode
 * @param fmt       - formatted msg
 * @param ...       - params
 *
 * @return success or error
 **/
wstk_status_t wstk_servlet_websock_send2(wstk_servlet_websock_t *servlet, uint32_t conn_id, websock_opcode_e opcode, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock = NULL;
    wstk_http_conn_t *http_conn = NULL;
    va_list ap;

    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(servlet->mutex);
    http_conn = wstk_core_inthash_find(servlet->sockets, conn_id);
    if(http_conn) {
        status = wstk_httpd_conn_take(http_conn);
    }
    wstk_mutex_unlock(servlet->mutex);

    if(status == WSTK_STATUS_SUCCESS) {
        status = wstk_tcp_srv_conn_socket(http_conn->tcp_conn, &sock);
        if(status == WSTK_STATUS_SUCCESS) {
            va_start(ap, fmt);
            status = wstk_websock_vsend(sock, opcode, 0, true, fmt, ap);
            va_end(ap);
        }

        wstk_httpd_conn_release(http_conn);
    }

    return status;
}
