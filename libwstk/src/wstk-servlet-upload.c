/**
 ** Simple upload servlet
 ** usage:
 **  curl -v http://127.0.0.1:8080/upload/ -H "Content-Type: multipart/form-data" -F file="@file.txt"
 **
 ** (C)2024 aks
 **/
#include <wstk-servlet-upload.h>
#include <wstk-log.h>
#include <wstk-httpd.h>
#include <wstk-pl.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-mbuf.h>
#include <wstk-regex.h>
#include <wstk-thread.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-hashtable.h>
#include <wstk-file.h>
#include <wstk-dir.h>
#include <multipartparser.h>

struct wstk_servlet_upload_s {
    wstk_mutex_t        *mutex;
    char                *upload_path;
    uint32_t            refs;
    uint32_t            content_max_len;
    bool                fl_destroyed;
    //
    wstk_servlet_upload_access_handler_t    access_handler;
    wstk_servlet_upload_accept_handler_t    accept_handler;
    wstk_servlet_upload_complete_handler_t  complete_handler;
};

typedef struct {
    wstk_mbuf_t     *body;
    char            *filename;
    uint32_t        cur_param;
    wstk_status_t   error;
} mp_parser_params_t;

static void desctuctor__wstk_servlet_upload_t(void *ptr) {
    wstk_servlet_upload_t *servlet = (wstk_servlet_upload_t *)ptr;
    bool floop = true;

    if(!servlet || servlet->fl_destroyed) {
        return;
    }
    servlet->fl_destroyed = true;

#ifdef WSTK_SERVLET_UPLOAD_DEBUG
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

    servlet->upload_path = wstk_mem_deref(servlet->upload_path);
    servlet->mutex = wstk_mem_deref(servlet->mutex);

#ifdef WSTK_SERVLET_UPLOAD_DEBUG
    WSTK_DBG_PRINT("servlet destroyed: servlet=%p", servlet);
#endif
}

static int mp_decoder_on_header_name(multipartparser *p, const char *data, size_t len) {
    mp_parser_params_t *params = (mp_parser_params_t *)p->data;
    wstk_pl_t src = {data, len};

    if(wstk_pl_strcasecmp(&src, "Content-Disposition") == 0) {
        params->cur_param = 1;
    } else if(wstk_pl_strcasecmp(&src, "Content-Type") == 0) {
        params->cur_param = 2;
    }

    return 0;
}

static int mp_decoder_on_header_value(multipartparser *p, const char *data, size_t len) {
    mp_parser_params_t *params = (mp_parser_params_t *)p->data;
    wstk_pl_t src = {data, len};
    wstk_pl_t field_name = { 0 };

    if(params->cur_param == 1) {
        if(wstk_regex(data, len, "form-data; name=\"file\"") == WSTK_STATUS_SUCCESS) {
            if(wstk_regex(data, len, "filename=\"[^\"]+\"", &field_name) == WSTK_STATUS_SUCCESS) {
                if(!params->filename) {
                    if(field_name.l > 1024) {
                        params->error = WSTK_STATUS_INVALID_PARAM;
                    } else {
                        params->error = wstk_pl_strdup(&params->filename, &field_name);
                    }
                }
            }
        }
    } else if(params->cur_param == 2) {
        params->cur_param = 10;
    }

    return params->error;
}

static int mp_decoder_on_part_data(multipartparser *p, const char *data, size_t len) {
    mp_parser_params_t *params = (mp_parser_params_t *)p->data;
    wstk_pl_t src = {data, len};

    if(params->cur_param == 10) {
        if(len > 0) {
            wstk_mbuf_write_pl(params->body, &src);
        }
    }

    return params->error;
}

static void servlet_perform_handler(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    wstk_servlet_upload_t *servlet = (wstk_servlet_upload_t *)udata;
    wstk_status_t status = 0;
    wstk_httpd_sec_ctx_t sec_ctx = {0};
    wstk_pl_t boundary = { 0 };
    wstk_mbuf_t *buffer = NULL;
    size_t body_len = 0;
    char *body_ptr = NULL;
    bool upl_allow = false;

    if(!servlet) {
        log_error("oops! (servlet == null)");
        wstk_httpd_ereply(conn, 500, NULL);
        return;
    }
    if(!msg) {
        log_error("oops! (msg == null)");
        wstk_httpd_ereply(conn, 500, NULL);
        return;
    }

    if(!servlet->upload_path) {
        log_error("Servlet not congigured (upload_path == NULL)");
        wstk_httpd_ereply(conn, 500, NULL);
        return;
    }

    if(servlet->content_max_len && msg->clen > servlet->content_max_len) {
        wstk_httpd_ereply(conn, 400, "Bad request (content is too big)");
        return;
    }

    if(wstk_pl_strcasecmp(&msg->method, "post") != 0) {
        wstk_httpd_ereply(conn, 400, "Bad request (expected method: POST)");
        return;
    }

    if(!wstk_pl_strstr(&msg->ctype, "multipart/form-data")) {
        wstk_httpd_ereply(conn, 400, "Bad request (expected content-type: multipart/form-data)");
        return;
    }

    /* authenticates and requests access */
    wstk_httpd_autheticate(conn, msg, &sec_ctx);
    if(servlet->access_handler) {
        upl_allow = servlet->access_handler(&sec_ctx);
    }
    if(!upl_allow) {
        log_error("Access restricted (conn=%p)", conn);
        wstk_httpd_ereply(conn, 401, NULL);
        goto out;
    }

    /* load the rest of the body */
    if(msg->clen > wstk_mbuf_left(conn->buffer)) {
        if(wstk_mbuf_alloc(&buffer, msg->clen) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate buffer");
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }
        if(wstk_httpd_conn_rdlock(conn, true) == WSTK_STATUS_SUCCESS) {
            while(true) {
                status = wstk_httpd_read(conn, buffer, WSTK_RD_TIMEOUT(msg->clen));
                if(servlet->fl_destroyed || buffer->pos >= msg->clen)  {
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
            log_error("Unable to read the whole body (status=%d)", (int)status);
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }
        body_ptr = (char *)buffer->buf;
        body_len = buffer->end;
    } else {
        body_ptr = (char *)wstk_mbuf_buf(conn->buffer);
        body_len = wstk_mbuf_left(conn->buffer);
    }

    if(!body_len) {
        log_error("No content (body_len == 0)");
        wstk_httpd_ereply(conn, 400, NULL);
        goto out;
    }

    /* parse form */
    if(wstk_regex(msg->ctype.p, msg->ctype.l, "boundary=[^]*", &boundary) == WSTK_STATUS_SUCCESS) {
        multipartparser mp_parser = { 0 };
        multipartparser_callbacks mp_callbacks = { 0 };
        mp_parser_params_t parser_params = {0};
        char *boundary_str = NULL;
        char *dst_path = NULL;
        size_t nparts = 0;

        if(wstk_mbuf_alloc(&parser_params.body, msg->clen) != WSTK_STATUS_SUCCESS) {
            log_error("mem fail");
            wstk_httpd_ereply(conn, 500, "Not enough memory");
            goto parser_out;
        }
        if(wstk_pl_strdup(&boundary_str, &boundary) != WSTK_STATUS_SUCCESS) {
            log_error("mem fail");
            wstk_httpd_ereply(conn, 500, "Not enough memory");
            goto parser_out;
        }

        /* setup parser */
        multipartparser_callbacks_init(&mp_callbacks);
        mp_callbacks.on_data = &mp_decoder_on_part_data;
        mp_callbacks.on_header_field = &mp_decoder_on_header_name;
        mp_callbacks.on_header_value = &mp_decoder_on_header_value;

        multipartparser_init(&mp_parser, boundary_str);
        mp_parser.data = &parser_params;

        /* parse form */
        nparts = multipartparser_execute(&mp_parser, &mp_callbacks, body_ptr, body_len);
        if(!nparts || parser_params.error != WSTK_STATUS_SUCCESS) {
            log_error("Unable to parse form (status=%d)", (int)parser_params.error);
            wstk_httpd_ereply(conn, 400, "Bad request (couldn't parse form)");
        } else {
            upl_allow = false; /* disallow by default */
            if(!wstk_httpd_is_valid_filename(parser_params.filename, wstk_str_len(parser_params.filename))) {
                wstk_httpd_ereply(conn, 400, "Bad request (malformed filename)");
            } else {
                if(servlet->accept_handler) {
                    upl_allow = servlet->accept_handler(&sec_ctx, parser_params.filename, wstk_mbuf_end(parser_params.body));
                }
                if(!upl_allow) {
                    wstk_httpd_ereply(conn, 403, NULL);
                } else {
                    status = wstk_file_name_concat(&dst_path, servlet->upload_path, parser_params.filename, WSTK_PATH_DELIMITER);
                    if(status != WSTK_STATUS_SUCCESS) {
                        log_error("Unable to store file (status=%d)", (int)status);
                        wstk_httpd_ereply(conn, 500, NULL);
                    } else {
                        wstk_mbuf_set_pos(parser_params.body, 0);
                        if((status = wstk_file_content_write(dst_path, parser_params.body, 0)) != WSTK_STATUS_SUCCESS) {
                            log_error("Unable to store file (status=%d)", (int)status);
                            wstk_httpd_ereply(conn, 500, NULL);
                        } else {
                            if(servlet->complete_handler) {
                                servlet->complete_handler(&sec_ctx, dst_path);
                            }
                            wstk_httpd_creply(conn, 200, NULL, "text/plain", "+OK");
                        }
                    }

                }
            }
        }
        parser_out:
        wstk_mem_deref(dst_path);
        wstk_mem_deref(boundary_str);
        wstk_mem_deref(parser_params.body);
        wstk_mem_deref(parser_params.filename);
        goto out;
    }

    /* default error */
    wstk_httpd_ereply(conn, 400, NULL);
out:
    wstk_httpd_sec_ctx_clean(&sec_ctx);
    wstk_mem_deref(buffer);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create and register upload servlet
 *
 * @param srv       - the server
 * @param path      - servlet path
 * @param servlet   - a new servlet instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_register_servlet_upload(wstk_httpd_t *srv, char *path, wstk_servlet_upload_t **servlet) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_servlet_upload_t *servlet_local = NULL;

    if(!servlet || !srv || !path) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&servlet_local, sizeof(wstk_servlet_upload_t), desctuctor__wstk_servlet_upload_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&servlet_local->mutex)) != WSTK_STATUS_SUCCESS) {
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
 * Configure servlet
 *
 * @param servlet           - the servlet
 * @param upload_path       - path fro upload files
 * @param content_max_len   - content max length (0 - no limit)
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_upload_configure(wstk_servlet_upload_t *servlet, char *upload_path, uint32_t content_max_len) {
    if(!servlet || !upload_path) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }
    if(servlet->upload_path) {
        log_warn("Servlet already configured");
        return WSTK_STATUS_FALSE;
    }

    if(!wstk_dir_exists(upload_path)) {
        log_error("Directory not found: %s", upload_path);
        return WSTK_STATUS_FALSE;
    }

    servlet->content_max_len = content_max_len;
    servlet->upload_path = wstk_str_dup(upload_path);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Access accept handler
 *
 * @param servlet - upload servlet
 * @param handler - the handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_upload_set_access_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_access_handler_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->access_handler = handler;
    return WSTK_STATUS_SUCCESS;
}

/**
 * File accept  handler
 *
 * @param servlet - upload servlet
 * @param handler - the handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_upload_set_accept_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_accept_handler_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->accept_handler = handler;
    return WSTK_STATUS_SUCCESS;
}

/**
 * On upload complete
 *
 * @param servlet - upload servlet
 * @param handler - the handler or NULL
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_upload_set_complete_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_complete_handler_t handler) {
    if(!servlet) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    servlet->complete_handler = handler;
    return WSTK_STATUS_SUCCESS;
}


