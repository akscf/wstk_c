/**
 **
 ** (C)2024 aks
 **/
#include <wstk-httpd.h>
#include <wstk-log.h>
#include <wstk-http-msg.h>
#include <wstk-tcp-srv.h>
#include <wstk-net.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-pl.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-regex.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-thread.h>
#include <wstk-worker.h>
#include <wstk-hashtable.h>
#include <wstk-base64.h>
#include <wstk-escape.h>
#include <wstk-time.h>
#include <wstk-file.h>
#include <wstk-dir.h>

#define HTTPD_READ_BUFFER_SIZE          8192
#define HTTPD_WRITE_BUFFER_SIZE         8192
#define HTTPD_BLOB_WRITE_BUFFER_SIZE    16384

#define HTTPD_DEFAULT_SERVER_ID         "wstk-httpd/1.x"
#define HTTPD_DEFAULT_CHARSET           "UTF-8"

#define HTTPD_ATTR__HTTPD_INSTANCE      "httpd"
#define HTTPD_ATTR__HTTP_CONNECTION     "http-conn"
#define HTTPD_ATTR__WEBSOCK_SERVLET     "ws-container"

static wstk_pl_t scheme_http = WSTK_PL("http");
static wstk_pl_t scheme_https = WSTK_PL("https");

struct wstk_httpd_s {
    wstk_mutex_t                        *mutex;
    wstk_hash_t                         *servlets;
    wstk_tcp_srv_t                      *tcp_server;
    const char                          *ident;
    char                                *charset;
    char                                *html_ctype;
    char                                *www_home;
    char                                *welcome_page;
    wstk_httpd_authentication_handler_t auth_handler;
    uint32_t                            id;
    uint32_t                            refs;
    bool                                allow_dir_browse;
    bool                                fl_destroyed;
    bool                                fl_ready;
};

typedef struct {
    wstk_mutex_t                        *mutex;
    wstk_httpd_t                        *server;
    char                                *path;
    void                                *udata;
    wstk_httpd_servlet_handler_t        handler;
    uint32_t                            refs;
    bool                                fl_destroyed;
    bool                                fl_adestroy_udata;
} servlet_container_t;

static void desctuctor__servlet_container_t(void *ptr) {
    servlet_container_t *container = (servlet_container_t *)ptr;
    bool floop = true;

    if(!container || container->fl_destroyed) {
        return;
    }
    container->fl_destroyed = true;

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("destroying container: container=%p (srv=%p, refs=%d, handler=%p, udate=%p, adestroy_udata=%d)", container, container->server, container->refs, container->handler, container->udata, container->fl_adestroy_udata);
#endif

    if(container->mutex) {
        while(floop) {
            wstk_mutex_lock(container->mutex);
            floop = (container->refs > 0);
            wstk_mutex_unlock(container->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(container->refs) {
        log_warn("Lost references (refs=%d)", container->refs);
    }

    if(container->udata && container->fl_adestroy_udata) {
        container->udata = wstk_mem_deref(container->udata);
    }

    container->path = wstk_mem_deref(container->path);
    container->mutex = wstk_mem_deref(container->mutex);

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("container destroyed: container=%p", container);
#endif
}

static void desctuctor__wstk_http_conn_t(void *ptr) {
    wstk_http_conn_t *conn = (wstk_http_conn_t *)ptr;

    if(!conn) {
        return;
    }
#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("destroying connection: http-conn=%p (srv=%p, tcp-conn=%p)", conn, conn->server, conn->tcp_conn);
#endif

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("connection destroyed: http-conn=%p", conn);
#endif
}

static void desctuctor__wstk_httpd_t(void *ptr) {
    wstk_httpd_t *srv = (wstk_httpd_t *)ptr;
    bool floop = true;

    if(!srv || srv->fl_destroyed) {
        return;
    }

    srv->fl_ready = false;
    srv->fl_destroyed = true;

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("destroying server: srv=%p (refs=%d)", srv, srv->refs);
#endif
    if(srv->tcp_server) {
        srv->tcp_server = wstk_mem_deref(srv->tcp_server);
    }

    if(srv->mutex) {
        while(floop) {
            wstk_mutex_lock(srv->mutex);
            floop = (srv->refs > 0);
            wstk_mutex_unlock(srv->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(srv->refs) {
        log_warn("Lost references (refs=%d)", srv->refs);
    }

    if(srv->servlets) {
        wstk_mutex_lock(srv->mutex);
        srv->servlets = wstk_mem_deref(srv->servlets);
        wstk_mutex_unlock(srv->mutex);
    }

    srv->welcome_page = wstk_mem_deref(srv->welcome_page);
    srv->www_home = wstk_mem_deref(srv->www_home);
    srv->charset = wstk_mem_deref(srv->charset);
    srv->html_ctype = wstk_mem_deref(srv->html_ctype);
    srv->mutex = wstk_mem_deref(srv->mutex);

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("server destroyed: srv=%p", srv);
#endif
}

static wstk_status_t scontainer_refs(servlet_container_t *container) {
    if(!container || container->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    if(container->mutex) {
        wstk_mutex_lock(container->mutex);
        container->refs++;
        wstk_mutex_unlock(container->mutex);
    }
    return WSTK_STATUS_SUCCESS;
}
static void scontainer_derefs(servlet_container_t *container) {
    if(!container)  { return; }
    if(container->mutex) {
        wstk_mutex_lock(container->mutex);
        if(container->refs > 0) container->refs--;
        wstk_mutex_unlock(container->mutex);
    }
}

static void tcp_handler(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    wstk_tcp_srv_t *tcp_srv = NULL;
    wstk_http_conn_t *http_conn = NULL;
    wstk_httpd_t *httpd = NULL;
    wstk_http_msg_t *http_msg = NULL;
    servlet_container_t *scontainer = NULL;
    wstk_file_meta_t file_meta = { 0 };
    wstk_file_t blobf = { 0 };
    wstk_httpd_ctype_info_t ctype_info = { 0 };
    char ctype_buffer_st[255] = {0};
    char *ctype_ptr = NULL;
    char *req_path = NULL, *req_file = NULL;
    bool fl_try_to_list_dir = false;

    wstk_tcp_srv_conn_server(conn, &tcp_srv);
    wstk_tcp_srv_attr_get(tcp_srv, HTTPD_ATTR__HTTPD_INSTANCE, (void *)&httpd);

    if(!httpd) {
        log_error("oops! (httpd == null)");
        return;
    }
    if(httpd->fl_destroyed) {
        return;
    }
    if(!wstk_mbuf_end(mbuf)) {
        return;
    }

    wstk_mutex_lock(httpd->mutex);
    httpd->refs++;
    wstk_mutex_unlock(httpd->mutex);

    wstk_tcp_srv_conn_attr_get(conn, HTTPD_ATTR__HTTP_CONNECTION, (void *)&http_conn);
    if(!http_conn) {
        if(wstk_mem_zalloc((void *)&http_conn, sizeof(wstk_http_conn_t), desctuctor__wstk_http_conn_t) != WSTK_STATUS_SUCCESS) {
            log_error("Unbable to allocate memory");
            wstk_tcp_srv_conn_close(conn);
            goto out;
        }
        if(wstk_tcp_srv_conn_attr_add(conn, HTTPD_ATTR__HTTP_CONNECTION, http_conn, true) == WSTK_STATUS_SUCCESS) {
            http_conn->buffer = mbuf;
            http_conn->server = httpd;
            http_conn->tcp_conn = conn;
            wstk_tcp_srv_conn_id(conn, &http_conn->conn_id);
        } else {
            log_error("Unbable to create a new http connection");
            wstk_tcp_srv_conn_close(conn);
            wstk_mem_deref(http_conn);
            goto out;
        }
    }

    /* is a websocket */
    if(http_conn->websock) {
        wstk_tcp_srv_conn_attr_get(conn, HTTPD_ATTR__WEBSOCK_SERVLET, (void *)&scontainer);
#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("performing websock: container=%p (http-conn=%p, handler=%p, udate=%p, path=%s)", scontainer, http_conn, scontainer->handler, scontainer->udata, scontainer->path);
#endif
        if(scontainer) {
            scontainer_refs(scontainer);
            scontainer->handler(http_conn, NULL, scontainer->udata);
            if(wstk_tcp_srv_conn_is_closed(http_conn->tcp_conn) || !http_conn->websock) {
                wstk_tcp_srv_conn_attr_del(conn, HTTPD_ATTR__WEBSOCK_SERVLET);
            }
            scontainer_derefs(scontainer);
        } else {
            log_error("Websocket corrupted (scontainer == null)");
            wstk_httpd_ereply(http_conn, 500, NULL);
        }
        goto out;
    }

    /* decode http message */
    if(wstk_http_msg_alloc(&http_msg) != WSTK_STATUS_SUCCESS) {
        log_error("Unbable to allocate memory (http_msg)");
        wstk_tcp_srv_conn_close(conn);
        goto out;
    }

    wstk_mbuf_set_pos(mbuf, 0);
    st = wstk_http_msg_decode(http_msg, mbuf);
    if(st != WSTK_STATUS_SUCCESS && st != WSTK_STATUS_NODATA) {
        log_error("Unbable to decode HTTP message (err=%d)", (int)st);
        wstk_tcp_srv_conn_close(conn);
        wstk_httpd_ereply(http_conn, 400, NULL);
        goto out;
    }

    /* update scheme */
    http_msg->scheme = (http_conn->tls ? scheme_https : scheme_http);

    /* slightly validate message */
    if(!http_msg->method.p || http_msg->method.l < 3) {
        wstk_tcp_srv_conn_close(conn);
        wstk_httpd_ereply(http_conn, 400, NULL);
        goto out;
    }

    if(wstk_unescape(&req_path, http_msg->path.p, http_msg->path.l) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to unescape request");
        wstk_tcp_srv_conn_close(conn);
        wstk_httpd_ereply(http_conn, 500, NULL);
        goto out;
    }

    /* lookup for servlet */
    wstk_mutex_lock(httpd->mutex);
    if(!wstk_hash_is_empty(httpd->servlets)) {
        char *name_ptr = req_path;

        /* skip exra lead slashes if exists (////...) */
        if(http_msg->path.l > 1 && name_ptr[1] == '/') {
            while(*name_ptr++) {
                if(*name_ptr != '/' || !*name_ptr) { break; }
            }
        }
        if(!*name_ptr) {
            wstk_tcp_srv_conn_close(conn);
            wstk_httpd_ereply(http_conn, 400, NULL);
            wstk_mutex_unlock(httpd->mutex); /* !!! */
            goto out;
        }

        /* first attempt */
        scontainer = wstk_hash_find(httpd->servlets, name_ptr);
        scontainer_refs(scontainer);

        /* plab-b */
        if(!scontainer) {
            char *items[5];
            char *req_path2 = NULL;
            wstk_mbuf_t *tbuf = NULL;
            uint32_t count = 0, i;

            /* skip lead slash */
            req_path2 = wstk_str_dup((*name_ptr == '/' ? ++name_ptr : name_ptr));
            count = wstk_str_separate(req_path2, '/', items, ARRAY_SIZE(items));
            if(count > 1) {
                if(wstk_mbuf_alloc(&tbuf, http_msg->path.l + 10) == WSTK_STATUS_SUCCESS) {
                    wstk_mbuf_write_u8(tbuf, '/');
                    for(i = 0; i < count; i++) {
                        wstk_mbuf_write_str(tbuf, items[i]);
                        wstk_mbuf_write_u8(tbuf, '/');
                        wstk_mbuf_write_u8(tbuf, 0x0);
                        if((scontainer = wstk_hash_find(httpd->servlets, (char *)tbuf->buf))) {
                            scontainer_refs(scontainer);
                            break;
                        }
                        tbuf->pos--;
                    }
                }
            }
            wstk_mem_deref(tbuf);
            wstk_mem_deref(req_path2);
        }
    }
    wstk_mutex_unlock(httpd->mutex);


    if(scontainer) {
#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("performing servlet: container=%p (http-conn=%p, handler=%p, udate=%p, path=%s)", scontainer, http_conn, scontainer->handler, scontainer->udata, req_path);
#endif
        scontainer->handler(http_conn, http_msg, scontainer->udata);
        if(http_conn->websock) {
            wstk_tcp_srv_conn_attr_add(conn, HTTPD_ATTR__WEBSOCK_SERVLET, scontainer, false);
        }
        scontainer_derefs(scontainer);
        goto out;
    }

    /* browse dir or show file */
    if(!httpd->www_home) {
        wstk_httpd_ereply(http_conn, 404, NULL);
        goto out;
    }

    if(http_msg->path.l == 1 && http_msg->path.p[0] == '/') {
        if(httpd->welcome_page) {
            if(wstk_file_name_concat(&req_file, httpd->www_home, httpd->welcome_page, WSTK_PATH_DELIMITER) != WSTK_STATUS_SUCCESS) {
                wstk_httpd_ereply(http_conn, 404, NULL);
                goto out;
            }
            if(!wstk_file_exists(req_file)) {
                wstk_mem_deref(req_file);
                req_file = wstk_str_dup(httpd->www_home);
            }
        } else {
            if(!httpd->allow_dir_browse) {
                wstk_httpd_ereply(http_conn, 404, NULL);
                goto out;
            }
        }
        fl_try_to_list_dir = true;
    } else {
        if(wstk_file_name_concat(&req_file, httpd->www_home, req_path, WSTK_PATH_DELIMITER) != WSTK_STATUS_SUCCESS) {
            wstk_httpd_ereply(http_conn, 404, NULL);
            goto out;
        }
    }

    if(!wstk_httpd_is_valid_path(req_file, strlen(req_file))) {
        wstk_httpd_ereply(http_conn, 404, NULL);
        goto out;
    }

    if(!wstk_file_exists(req_file)) {
        if(httpd->allow_dir_browse) {
            fl_try_to_list_dir = wstk_dir_exists(req_file);
        }
        if(!fl_try_to_list_dir) {
            wstk_httpd_ereply(http_conn, 404, NULL);
            goto out;
        }
    } else {
        fl_try_to_list_dir = false;
    }

    if(wstk_pl_strcasecmp(&http_msg->method, "GET") != 0) {
        wstk_httpd_ereply(http_conn, 405, "Expected method: GET");
        goto out;
    }

    if(fl_try_to_list_dir) {
        wstk_tcp_srv_conn_close(conn); /* cause we don't have content lenght */
        if(wstk_httpd_browse_dir(http_conn, req_file, req_path, httpd->html_ctype, (http_msg->charset.l ? &http_msg->charset : NULL)) != WSTK_STATUS_SUCCESS) {
            wstk_httpd_ereply(http_conn, 500, NULL);
            goto out;
        }
    } else {
        if(wstk_file_get_meta(req_file, &file_meta) != WSTK_STATUS_SUCCESS) {
            wstk_httpd_ereply(http_conn, 404, NULL);
            goto out;
        }

        if(wstk_file_open(&blobf, req_file, "rb") != WSTK_STATUS_SUCCESS) {
            wstk_httpd_ereply(http_conn, 404, NULL);
            goto out;
        }


        wstk_httpd_content_type_by_file_ext(&ctype_info, req_file);
        if(!ctype_info.binary) {
            wstk_snprintf(ctype_buffer_st, sizeof(ctype_buffer_st), "%s; charset=%s", ctype_info.ctype, httpd->charset);
            ctype_ptr = (char *)ctype_buffer_st;
        } else {
            ctype_ptr = (char *)ctype_info.ctype;
        }

        if(wstk_httpd_breply(http_conn, 200, NULL, ctype_ptr, file_meta.size, file_meta.mtime, (wstk_httpd_blob_reader_callback_t)wstk_file_read, (void *)&blobf) != WSTK_STATUS_SUCCESS) {
            wstk_httpd_ereply(http_conn, 404, NULL);
        }

        wstk_file_close(&blobf);
#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("file sent: name=%s, size=%d", req_file, (int)file_meta.size);
#endif
    }

out:
    wstk_mem_deref(http_msg);
    wstk_mem_deref(req_path);
    wstk_mem_deref(req_file);

    wstk_mutex_lock(httpd->mutex);
    if(httpd->refs) httpd->refs--;
    wstk_mutex_unlock(httpd->mutex);
}

static wstk_status_t http_vreply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *fmt, va_list ap) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_socket_t *sock = NULL;
    const char *servert_ident = (conn->server->ident ? conn->server->ident : HTTPD_DEFAULT_SERVER_ID);
    char tbuff[128] = {0};

    if(!conn || !conn->server) {
        return WSTK_STATUS_FALSE;
    }

    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if((status = wstk_tcp_srv_conn_socket(conn->tcp_conn, &sock)) != WSTK_STATUS_SUCCESS) {
        return status;
    }
    if(!sock) {
        return WSTK_STATUS_FALSE;
    }

    if((status = wstk_time_to_str_rfc822(0, (char *)tbuff, sizeof(tbuff))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_tcp_printf(sock,
                             "HTTP/1.1 %u %s\r\n"
                             "Server: %s\r\n"
                             "Date: %s\r\n",
                             scode, reason,
                             servert_ident,
                             (char *)tbuff
            );
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(fmt) {
        status = wstk_tcp_vprintf(sock, fmt, ap);
    } else {
        status = wstk_tcp_printf(sock, "Content-Length: 0\r\n\r\n");
    }
out:
    return status;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new httpd
 *
 * @param srv               - a new server instance
 * @param address           - address for listening
 * @param max_conns         - max connections
 * @param max_idle          - connection idle in seconds
 * @param charset           - server charset (default UTF-8)
 * @param www_home          - null or home path
 * @param welcome_page      - null or something like index.html
 * @param allow_dir_browse  - if no index file show directory content
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_create(wstk_httpd_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t max_idle, char *charset, char *www_home, char *welcome_page, bool allow_dir_browse) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_httpd_t *srv_local = NULL;

    if(!srv || !address) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&srv_local, sizeof(wstk_httpd_t), desctuctor__wstk_httpd_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_mutex_create(&srv_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_tcp_srv_create((void *)&srv_local->tcp_server, address, max_conns, max_idle, HTTPD_READ_BUFFER_SIZE, tcp_handler)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_hash_init(&srv_local->servlets)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(wstk_str_is_empty(charset)) {
        srv_local->charset = wstk_str_dup(HTTPD_DEFAULT_CHARSET);
    } else {
        srv_local->charset = wstk_str_dup(charset);
    }

    status = wstk_sdprintf(&srv_local->html_ctype, "text/html; charset=%s", srv_local->charset);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if(www_home) {
        if(!wstk_dir_exists(www_home)) {
            log_error("Directory not found: %s", www_home);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
        srv_local->www_home = wstk_str_dup(www_home);
    }
    if(welcome_page) {
        srv_local->welcome_page = wstk_str_dup(welcome_page);
    }

    srv_local->allow_dir_browse = (www_home ? allow_dir_browse : false);

    wstk_tcp_srv_id(srv_local->tcp_server, &srv_local->id);
    wstk_tcp_srv_attr_add(srv_local->tcp_server, HTTPD_ATTR__HTTPD_INSTANCE, srv_local, false);

    *srv = srv_local;

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("httpd created: server=%p (id=0x%x, charset=%s, www_home=%s, welcome_page=%s, allow_dir_browse=%d)", srv_local, srv_local->id, srv_local->charset, srv_local->www_home, srv_local->welcome_page, allow_dir_browse);
#endif
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(srv_local);
    }
    return status;
}

/**
 * Create a new httpd (ssl)
 *
 * @param srv               - a new server instance
 * @param address           - address for listening
 * @param cert              - certificate file
 * @param max_conns         - max connections
 * @param max_idle          - connection idle in seconds
 * @param charset           - server charset (default UTF-8)
 * @param www_home          - null or home path
 * @param welcome_page      - null or something like index.html
 * @param allow_dir_browse  - if no index file show directory content
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpsd_create(wstk_httpd_t **srv, wstk_sockaddr_t *address, char *cert, uint32_t max_conns, uint32_t max_idle, char *charset, char *www_home, char *welcome_page, bool allow_dir_browse) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_httpd_t *srv_local = NULL;

    if(!srv || !address || !cert) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&srv_local, sizeof(wstk_httpd_t), desctuctor__wstk_httpd_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_mutex_create(&srv_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_tcp_ssl_srv_create((void *)&srv_local->tcp_server, address, cert, max_conns, max_idle, HTTPD_READ_BUFFER_SIZE, tcp_handler)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_hash_init(&srv_local->servlets)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(wstk_str_is_empty(charset)) {
        srv_local->charset = wstk_str_dup(HTTPD_DEFAULT_CHARSET);
    } else {
        srv_local->charset = wstk_str_dup(charset);
    }

    status = wstk_sdprintf(&srv_local->html_ctype, "text/html; charset=%s", srv_local->charset);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if(www_home) {
        if(!wstk_dir_exists(www_home)) {
            log_error("Directory not found: %s", www_home);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
        srv_local->www_home = wstk_str_dup(www_home);
    }
    if(welcome_page) {
        srv_local->welcome_page = wstk_str_dup(welcome_page);
    }

    srv_local->allow_dir_browse = (www_home ? allow_dir_browse : false);

    wstk_tcp_srv_id(srv_local->tcp_server, &srv_local->id);
    wstk_tcp_srv_attr_add(srv_local->tcp_server, HTTPD_ATTR__HTTPD_INSTANCE, srv_local, false);

    *srv = srv_local;

#ifdef WSTK_HTTPD_DEBUG
    WSTK_DBG_PRINT("httpsd created: server=%p (id=0x%x, charset=%s, www_home=%s, welcome_page=%s, allow_dir_browse=%d)", srv_local, srv_local->id, srv_local->charset, srv_local->www_home, srv_local->welcome_page, allow_dir_browse);
#endif
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(srv_local);
    }
    return status;
}

/**
 * Start the instance
 *
 * @param srv - the server instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_start(wstk_httpd_t *srv) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }
    if(srv->fl_ready) {
        return WSTK_STATUS_SUCCESS;
    }

    status = wstk_tcp_srv_start(srv->tcp_server);
    if(status == WSTK_STATUS_SUCCESS) {
        srv->fl_ready = true;
    }

#ifdef WSTK_HTTPD_DEBUG
    if(status == WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("server started: srv=%p (tcp-srv=%p)", srv, srv->tcp_server);
    }
#endif

    return status;
}

/**
 * Server state
 *
 * @param srv - the server instance
 *
 * @return true/false
 **/
bool wstk_httpd_is_destroyed(wstk_httpd_t *srv) {
    if(!srv || srv->fl_destroyed) {
        return true;
    }
    return srv->fl_destroyed;
}

/**
 * Resgister servlet
 *
 * @param srv           - server instance
 * @param path          - servlet path
 * @param handler       - request handler
 * @param udata         - user data
 * @param auto_destroy  - if true udata will be auto detroyed
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_register_servlet(wstk_httpd_t *srv, const char *path, wstk_httpd_servlet_handler_t handler, void *udata, bool auto_destroy) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    servlet_container_t *container = NULL;

    if(!srv || !path || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(srv->mutex);
    container = wstk_hash_find(srv->servlets, path);
    if(!container) {
        status = wstk_mem_zalloc((void *)&container, sizeof(servlet_container_t), desctuctor__servlet_container_t);
        if(status == WSTK_STATUS_SUCCESS) {
            container->server = srv;
            container->handler = handler;
            container->path = wstk_str_dup(path);
            container->udata = udata;
            container->fl_adestroy_udata = auto_destroy;

            status = wstk_mutex_create(&container->mutex);
            if(status == WSTK_STATUS_SUCCESS) {
                status = wstk_hash_insert_ex(srv->servlets, container->path, container, true);
            }
        }
    } else {
        status = WSTK_STATUS_ALREADY_EXISTS;
    }
    wstk_mutex_unlock(srv->mutex);

    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(container);
    } else {
#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("servlet registered: container=%p (path=%s, udata=%p, destroy_udata=%d)", container, container->path, container->udata, container->fl_adestroy_udata);
#endif
    }
    return status;
}

/**
 * Unresgister and destroy servlet
 *
 * @param srv   - server instance
 * @param path  - servlet path
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_unregister_servlet(wstk_httpd_t *srv, const char *path) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!srv || !path) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mutex_lock(srv->mutex);
    wstk_hash_delete(srv->servlets, path);
    wstk_mutex_unlock(srv->mutex);

    return status;
}

/**
 * HTTP reply
 *
 * @param conn      - the connection
 * @param scode     - ret code
 * @param reason    - reson msg
 * @param fmt       * format string
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_reply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    va_list ap;

    va_start(ap, fmt);
    status = http_vreply(conn, scode, reason, fmt, ap);
    va_end(ap);

    return status;
}

/**
 * Error reply
 *
 * @param conn      - the connection
 * @param scode     - http code
 * @param reason    - http msg
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_ereply(wstk_http_conn_t *conn, uint32_t scode, const char *reason) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_httpd_t *srv = (conn ? conn->server : NULL);
    const char *keep_alive_str;
    const char *reason_local = (reason ? reason : wstk_httpd_reason_by_code(scode));
    wstk_mbuf_t *mbuf = NULL;
    char tbuff[128] = {0};

    if(!conn || !conn->server) {
        return WSTK_STATUS_FALSE;
    }
    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if((status = wstk_mbuf_alloc(&mbuf, HTTPD_WRITE_BUFFER_SIZE)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    wstk_mbuf_printf(mbuf,
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head><title>%u %s</title></head>\n"
                "<body><h2>%u %s</h2></body>\n"
                "</html>\n",
                scode, reason_local,
                scode, reason_local
    );

    if((status = wstk_time_to_str_rfc822(0, (char *)tbuff, sizeof(tbuff))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    keep_alive_str = (wstk_tcp_srv_conn_is_closed(conn->tcp_conn) ? "close" : "keep-alive");
    status = wstk_httpd_reply(conn, scode, reason_local,
                "Last-Modified: %s\r\n"
                "Connection: %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "\r\n"
                "%b",
                (char *)tbuff,
                keep_alive_str,
                srv->html_ctype,
                mbuf->end,
                mbuf->buf, mbuf->end
            );
out:
    wstk_mem_deref(mbuf);
    return status;
}

/**
 * Content reply
 *
 * @param conn      - the connection
 * @param scode     - http code
 * @param reason    - http msg
 * @param ctype     - content type
 * @param fmt       - formatted string
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_creply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *ctype, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *mbuf = NULL;
    char tbuff[128] = {0};
    const char *reason_local = (reason ? reason : wstk_httpd_reason_by_code(scode));
    const char *keep_alive_str;
    va_list ap;

    if(!conn || !conn->server) {
        return WSTK_STATUS_FALSE;
    }
    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if((status = wstk_mbuf_alloc(&mbuf, HTTPD_WRITE_BUFFER_SIZE)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    va_start(ap, fmt);
    status = wstk_mbuf_vprintf(mbuf, fmt, ap);
    va_end(ap);

    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_time_to_str_rfc822(0, (char *)tbuff, sizeof(tbuff))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    keep_alive_str = (wstk_tcp_srv_conn_is_closed(conn->tcp_conn) ? "close" : "keep-alive");
    status = wstk_httpd_reply(conn, scode, reason_local,
                "Last-Modified: %s\r\n"
                "Connection: %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "\r\n"
                "%b",
                (char *)tbuff,
                keep_alive_str,
                ctype,
                mbuf->end,
                mbuf->buf, mbuf->end
            );
out:
    wstk_mem_deref(mbuf);
    return status;
}

/**
 * BLOB reply
 *
 * @param conn      - the connection
 * @param scode     - http code
 * @param reason    - http msg
 * @param ctype     - content type
 * @param blen      - blob length
 * @param mtime     - modified time
 * @param rcallback - reader callback
 * @param udata     - reader function user data
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_breply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *ctype, size_t blen, time_t mtime, wstk_httpd_blob_reader_callback_t rcallback, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    const char *keep_alive_str;
    const char *reason_local = (reason ? reason : wstk_httpd_reason_by_code(scode));
    wstk_mbuf_t *mbuf = NULL;
    wstk_socket_t *sock = NULL;
    char tbuff[128] = {0};

    if(!conn || !conn->server || !rcallback) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if((status = wstk_tcp_srv_conn_socket(conn->tcp_conn, &sock)) != WSTK_STATUS_SUCCESS) {
        return status;
    }
    if(!sock) {
        return WSTK_STATUS_FALSE;
    }

    // send header
    if((status = wstk_time_to_str_rfc822(mtime, (char *)tbuff, sizeof(tbuff))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    keep_alive_str = (wstk_tcp_srv_conn_is_closed(conn->tcp_conn) ? "close" : "keep-alive");
    status = wstk_httpd_reply(conn, scode, reason_local,
                "Last-Modified: %s\r\n"
                "Connection: %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "\r\n",
                (char *)tbuff,
                keep_alive_str,
                ctype,
                blen
            );
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(!blen) {
        goto out;
    }

    // send blob
    if((status = wstk_mbuf_alloc(&mbuf, HTTPD_BLOB_WRITE_BUFFER_SIZE)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    while(true) {
        wstk_mbuf_clean(mbuf);

        status = rcallback(udata, mbuf);
        if(status == WSTK_STATUS_NODATA)  { status = WSTK_STATUS_SUCCESS; break; }
        if(status != WSTK_STATUS_SUCCESS) { break; }
        if(!wstk_mbuf_pos(mbuf) && !wstk_mbuf_end(mbuf)) { break; }

        wstk_mbuf_set_pos(mbuf, 0);
        status = wstk_tcp_write(sock, mbuf, WSTK_WR_TIMEOUT(blen));
        if(status != WSTK_STATUS_SUCCESS) {
            wstk_tcp_srv_conn_close(conn->tcp_conn);
            break;
        }
    }
out:
    wstk_mem_deref(mbuf);
    return status;
}

/**
 * Read data from the connection
 *
 * @param conn      - the connection
 * @param mbuf      - the buffer
 * @param timeout   - timeout in sec
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_read(wstk_http_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!conn || !conn->buffer || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(conn->buffer->pos < conn->buffer->end) {
        wstk_mbuf_write_mem(mbuf, wstk_mbuf_buf(conn->buffer), wstk_mbuf_left(conn->buffer));
        conn->buffer->pos = conn->buffer->end;
        status = WSTK_STATUS_SUCCESS;
    } else {
        status = wstk_tcp_srv_conn_read(conn->tcp_conn, mbuf, timeout);
    }

    return status;
}

/**
 * See wstk_tcp_srv_conn_rdlock()
 *
 * @param conn      - the connection
 * @param flag      - the conection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_conn_rdlock(wstk_http_conn_t *conn, bool flag) {
    if(!conn) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(conn->server->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    return wstk_tcp_srv_conn_rdlock(conn->tcp_conn, flag);
}

/**
 * Lookup http connection (it will be locked if found)
 * Don't forget about: wstk_httpd_conn_release()
 *
 * @param srv       - the server
 * @param id        - connection id
 *
 * @return conn or NULL
 **/
wstk_http_conn_t *wstk_httpd_conn_lookup(wstk_httpd_t *srv, uint32_t id) {
    wstk_tcp_srv_conn_t *tcp_conn = NULL;
    wstk_http_conn_t *http_conn = NULL;

    if(!srv || srv->fl_destroyed) {
        return NULL;
    }

    tcp_conn = wstk_tcp_srv_lookup_conn(srv->tcp_server, id);
    if(tcp_conn) {
        wstk_tcp_srv_conn_attr_get(tcp_conn, HTTPD_ATTR__HTTP_CONNECTION, (void *)&http_conn);
    }

    return http_conn;
}

/**
 * Marked connection as taken
 * (increase refs)
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_conn_take(wstk_http_conn_t *conn) {
    wstk_tcp_srv_conn_t *tcp_conn = (conn ? conn->tcp_conn : NULL);

    return wstk_tcp_srv_conn_take(tcp_conn);
}

/**
 * Release connection
 * (decreased refs)
 *
 * @param conn  - the connection
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_conn_release(wstk_http_conn_t *conn) {
    wstk_tcp_srv_conn_t *tcp_conn = (conn ? conn->tcp_conn : NULL);

    return wstk_tcp_srv_conn_release(tcp_conn);
}

/**
 * Set server identifier
 *
 * @param srv           - the server
 * @param server_name   - name like: serverName/x.y
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_set_ident(wstk_httpd_t *srv, const char *server_name) {
    if(!srv) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    srv->ident = server_name;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Set authenticator
 *
 * @param srv       - the server
 * @param handler   - authentication handler
 * @param replace   - force replace
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_set_authenticator(wstk_httpd_t *srv, wstk_httpd_authentication_handler_t handler, bool replace) {
    if(!srv || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(srv->auth_handler && !replace) {
#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("authenticator already installed: handler=%p", srv->auth_handler);
#endif
        return WSTK_STATUS_ALREADY_EXISTS;
    }

    srv->auth_handler = handler;

#ifdef WSTK_HTTPD_DEBUG
        WSTK_DBG_PRINT("authenticator installed: handler=%p", srv->auth_handler);
#endif
    return WSTK_STATUS_SUCCESS;
}

/**
 * Authenticate (supports: Basic and Bearer)
 *
 * @param conn  - the connection
 * @param msg   - the message
 * @param ctx   - security context
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_autheticate(wstk_http_conn_t *conn, wstk_http_msg_t *msg, wstk_httpd_sec_ctx_t *ctx) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_sockaddr_t *peer = NULL;
    const char *hdr_auth=NULL, *hdr_sid=NULL, *hdr_tok=NULL;
    const char *clogin = NULL, *cpass = NULL, *ctoken = NULL;
    wstk_httpd_auth_request_t auth_req = {0};
    wstk_httpd_auth_response_t auth_rsp = {0};
    char *xtoken = NULL, *xsession = NULL;
    char *bbuf = NULL;
    size_t blen = 0;

    if(!conn || !msg || !ctx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    ctx->peer = NULL;
    ctx->token = NULL;
    ctx->session = NULL;
    ctx->user_identity = NULL;

    if(!conn->server->auth_handler) {
        return WSTK_STATUS_FALSE;
    }

    wstk_tcp_srv_conn_peer(conn->tcp_conn, &peer);
    wstk_http_msg_header_get(msg, "X-SESSION-ID", &hdr_sid);

    /* workaround for websocket */
    if(wstk_http_msg_header_get(msg, "Sec-WebSocket-Protocol", &hdr_tok) == WSTK_STATUS_SUCCESS) {
        if(hdr_tok) {
            wstk_pl_t p1 = {0};
            if(wstk_regex(hdr_tok, strlen(hdr_tok), "x-session, [^]+", &p1) == WSTK_STATUS_SUCCESS) {
                wstk_pl_strdup(&xtoken, &p1);
            } else if(wstk_regex(hdr_tok, strlen(hdr_tok), "x-token, [^]+", &p1) == WSTK_STATUS_SUCCESS) {
                wstk_pl_strdup(&xtoken, &p1);
            }
        }
    }

    if(wstk_http_msg_header_get(msg, "Authorization", &hdr_auth) == WSTK_STATUS_SUCCESS && hdr_auth) {
        wstk_pl_t p1 = {0};
        if(wstk_regex(hdr_auth, strlen(hdr_auth), "Basic [^]+", &p1) == WSTK_STATUS_SUCCESS) {
            if(wstk_base64_decode_str(p1.p, p1.l, &bbuf, &blen) == WSTK_STATUS_SUCCESS) {
                uint32_t x = 0;
                for(x=0; x <= blen; x++) {
                    if(bbuf[x] == ':') { bbuf[x] = '\0'; break; }
                }
                if(x && x < blen) {
                    clogin = (char *)bbuf;
                    cpass = (char *)bbuf + x + 1;
                }
            }
        } else if(wstk_regex(hdr_auth, strlen(hdr_auth), "Bearer [^]+", &p1) == WSTK_STATUS_SUCCESS) {
            if(wstk_pl_strdup(&bbuf, &p1) == WSTK_STATUS_SUCCESS) {
                ctoken = bbuf;
            }
        }
    }

    auth_req.msg = msg;
    auth_req.conn = conn;
    auth_req.peer = peer;
    auth_req.session = (hdr_sid ? hdr_sid : xsession);
    auth_req.token = (ctoken ? ctoken : xtoken);
    auth_req.login = clogin;
    auth_req.password = cpass;

    conn->server->auth_handler(&auth_req, &auth_rsp);

    // fill-in ctx
    ctx->peer = auth_req.peer;                      // refs
    ctx->token = wstk_str_dup(auth_req.token);      // clone
    ctx->session = wstk_str_dup(auth_req.session);  // clone
    ctx->user_identity = auth_rsp.user_identity;    // refs
    ctx->destroy_identity = auth_rsp.destroy_identity;
    ctx->permitted = auth_rsp.permitted;
    ctx->user_id = auth_rsp.user_id;
    ctx->role = auth_rsp.role;

    wstk_mem_deref(bbuf);
    wstk_mem_deref(xtoken);
    wstk_mem_deref(xsession);

    return status;
}

/**
 * Get server charset
 *
 * @param srv       - the server
 * @param charset   - refs to charset
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_charset(wstk_httpd_t *srv, const char **charset) {
   if(!srv || !charset) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *charset = srv->charset;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Get listen address
 *
 * @param srv       - the server
 * @param charset   - refs to charset
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_listen_address(wstk_httpd_t *srv, wstk_sockaddr_t **laddr) {
   if(!srv || !laddr) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(srv->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    return wstk_tcp_srv_listen_address(srv->tcp_server, laddr);
}
