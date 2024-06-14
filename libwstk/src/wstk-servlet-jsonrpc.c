/**
 ** JSON-RPC servlet
 **
 ** (C)2024 aks
 **/
#include <wstk-servlet-jsonrpc.h>
#include <wstk-log.h>
#include <wstk-httpd.h>
#include <wstk-hashtable.h>
#include <wstk-pl.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-mbuf.h>
#include <wstk-thread.h>
#include <wstk-mutex.h>
#include <wstk-sleep.h>
#include <wstk-time.h>
#include <cJSON.h>
#include <cJSON_Utils.h>

#define JSRPC_CONTENT_MAX_LENGTH  1048576 // 1Mb
#define JSRPC_CONTENT_TYPE_FMT    "application/json; charset=%s"

struct wstk_servlet_jsonrpc_s {
    wstk_mutex_t     *mutex;
    wstk_hash_t      *services;
    char             *ctype;
    uint32_t         refs;
    bool             fl_destroyed;
};

struct wstk_servlet_jsonrpc_handler_result_s {
    cJSON            *obj;
    bool             error;
};

typedef struct service_entry_s {
    wstk_mutex_t     *mutex;
    char             *name;
    void             *udata;
    uint32_t         refs;
    bool             fl_destroyed;
    bool             fl_adestroy_udata;
    //
    wstk_servlet_jsonrpc_service_handler_t  hnadler;
} service_entry_t;


static wstk_status_t sentry_refs(service_entry_t *entry) {
    if(!entry || entry->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    if(entry->mutex) {
        wstk_mutex_lock(entry->mutex);
        entry->refs++;
        wstk_mutex_unlock(entry->mutex);
    }
    return WSTK_STATUS_SUCCESS;
}
static void sentry_derefs(service_entry_t *entry) {
    if(!entry)  { return; }
    if(entry->mutex) {
        wstk_mutex_lock(entry->mutex);
        if(entry->refs > 0) entry->refs--;
        wstk_mutex_unlock(entry->mutex);
    }
}

static void desctuctor__service_entry_t(void *ptr) {
    service_entry_t *entry = (service_entry_t *)ptr;
    bool floop = true;

    if(!entry || entry->fl_destroyed) {
        return;
    }
    entry->fl_destroyed = true;

#ifdef WSTK_SERVLET_JSONRPC_DEBUG
    WSTK_DBG_PRINT("destroying service: service=%p (name=%s, refs=%d, udata=%p, destroy_udata=%d)", entry, entry->name, entry->refs, entry->udata, entry->fl_adestroy_udata);
#endif

    if(entry->mutex) {
        while(floop) {
            wstk_mutex_lock(entry->mutex);
            floop = (entry->refs > 0);
            wstk_mutex_unlock(entry->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(entry->refs) {
        log_warn("Lost references (refs=%d)", entry->refs);
    }

    if(entry->udata && entry->fl_adestroy_udata) {
        entry->udata = wstk_mem_deref(entry->udata);
    }

    entry->name = wstk_mem_deref(entry->name);

#ifdef WSTK_SERVLET_JSONRPC_DEBUG
    WSTK_DBG_PRINT("service destroyed: service=%p", entry);
#endif
}

static void desctuctor__wstk_servlet_jsonrpc_t(void *ptr) {
    wstk_servlet_jsonrpc_t *servlet = (wstk_servlet_jsonrpc_t *)ptr;
    bool floop = true;

    if(!servlet || servlet->fl_destroyed) {
        return;
    }
    servlet->fl_destroyed = true;

#ifdef WSTK_SERVLET_JSONRPC_DEBUG
    WSTK_DBG_PRINT("destroying servlet: servlet=%p (refs=%d)", servlet, servlet->refs);
#endif

    if(servlet->mutex) {
        while(floop) {
            wstk_mutex_lock(servlet->mutex);
            floop = (servlet->refs > 0);
            wstk_mutex_unlock(servlet->mutex);

            wstk_msleep(250);
        }
    }

    if(servlet->refs) {
        log_warn("Lost references (refs=%d)", servlet->refs);
    }

    if(servlet->services) {
        servlet->services = wstk_mem_deref(servlet->services);
    }

    servlet->ctype = wstk_mem_deref(servlet->ctype);
    servlet->mutex = wstk_mem_deref(servlet->mutex);

#ifdef WSTK_SERVLET_JSONRPC_DEBUG
    WSTK_DBG_PRINT("servlet destroyed: servlet=%p", servlet);
#endif
}

static cJSON *rpc_create_error(uint32_t origin, uint32_t code, char *message) {
    cJSON *obj = NULL;

    obj = cJSON_CreateObject();
    if(!obj) {
        log_error("Unable to crate json object");
        return NULL;
    }

    cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(code));
    cJSON_AddItemToObject(obj, "origin", cJSON_CreateNumber(origin));
    cJSON_AddItemToObject(obj, "message", (message ? cJSON_CreateString(message) : cJSON_CreateNull()) );

    return obj;
}

static void rpc_write_response(wstk_servlet_jsonrpc_t *servlet, wstk_http_conn_t *conn, cJSON *data) {
    char *json_text = NULL;

    if(!conn || !data) {
        return;
    }
    if(wstk_tcp_srv_conn_is_destroyed(conn->tcp_conn)) {
        return;
    }

    json_text = cJSON_PrintUnformatted(data);
    if(json_text) {
        wstk_httpd_creply(conn, 200, NULL, servlet->ctype, "%s", json_text);
#ifdef WSTK_SERVLET_JSONRPC_DEBUG
        WSTK_DBG_PRINT("rpc-resonse: %s", json_text);
#endif
        free(json_text);
    }
}

static void dump_request(wstk_mbuf_t *mbuf, bool zpos) {
#ifdef WSTK_SERVLET_JSONRPC_DEBUG
    char *str = NULL;
    if(zpos) { wstk_mbuf_set_pos(mbuf, 0); }
    str = wstk_str_ndup((const char *)wstk_mbuf_buf(mbuf), wstk_mbuf_end(mbuf));
    WSTK_DBG_PRINT("rpc-request: %s", str);
    wstk_mem_deref(str);
#endif
}

static void servlet_perform_handler(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_servlet_jsonrpc_t *servlet = (wstk_servlet_jsonrpc_t *)udata;
    const char *http_err_msg;
    uint32_t http_err_code = 0;
    wstk_mbuf_t *buffer = NULL;
    const cJSON *rpc_id, *rpc_service, *rpc_method, *rpc_params;
    wstk_httpd_sec_ctx_t sec_ctx = {0};
    service_entry_t *service_entry = NULL;
    wstk_servlet_jsonrpc_handler_result_t *hresult = NULL;
    cJSON *js_req = NULL;
    cJSON *js_rsp = NULL;
    cJSON *js_error = NULL;

    if(!servlet) {
        log_error("oops! (servlet == null)");
        http_err_code = 500;
        goto out;
    }
    if(!msg) {
        log_error("oops! (msg == null)");
        http_err_code = 500;
        goto out;
    }

    if(msg->ctype.p && wstk_pl_strcasecmp(&msg->ctype, "application/json")) {
        http_err_code = 400;
        goto out;
    }
    if(!msg->clen) {
        http_err_code = 400;
        goto out;
    }
    if(msg->clen > JSRPC_CONTENT_MAX_LENGTH) {
        log_error("Request is too big (%d > %d)",  msg->clen, JSRPC_CONTENT_MAX_LENGTH);
        http_err_code = 400;
        goto out;
    }

    /* read and parse request */
    if(msg->clen > wstk_mbuf_left(conn->buffer)) {
        if(wstk_mbuf_alloc(&buffer, msg->clen) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate buffer");
            http_err_code = 500;
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
            log_error("Unable to read the whole content (status=%d)", (int)status);
            http_err_code = 500;
            goto out;
        }
        dump_request(buffer, true);
        js_req = cJSON_ParseWithLength((const char *)buffer->buf, buffer->end);
    } else {
        if(conn->buffer && conn->buffer->pos) {
            dump_request(conn->buffer, false);
            js_req = cJSON_ParseWithLength((const char *)wstk_mbuf_buf(conn->buffer), wstk_mbuf_left(conn->buffer));
        }
    }

    if(!js_req) {
        log_error("Unable to decode json");
        http_err_code = 400;
        goto out;
    }

    // validate request
    rpc_id = cJSON_GetObjectItem(js_req, "id");
    rpc_service = cJSON_GetObjectItem(js_req, "service");
    rpc_method = cJSON_GetObjectItem(js_req, "method");
    rpc_params = cJSON_GetObjectItem(js_req, "params");

    if(!cJSON_IsNumber(rpc_id)) {
        http_err_msg = "JSON-RPC: Malformed id";
        http_err_code = 400;
        goto out;
    }
    if(!cJSON_IsString(rpc_service) || !rpc_service->valuestring) {
        http_err_msg = "JSON-RPC: Malformed service name";
        http_err_code = 400;
        goto out;
    }
    if(!cJSON_IsString(rpc_method) || !rpc_method->valuestring) {
        http_err_msg = "JSON-RPC: Malformed method name";
        http_err_code = 400;
        goto out;
    }
    if(!cJSON_IsArray(rpc_params)) {
        http_err_msg = "JSON-RPC: Malformed params";
        http_err_code = 400;
        goto out;
    }

    // create response
    js_rsp = cJSON_CreateObject();
    if(!js_rsp) {
        log_error("Unable to crate json object");
        http_err_code = 500;
        goto out;
    }

    wstk_mutex_lock(servlet->mutex);
    service_entry = wstk_hash_find(servlet->services, rpc_service->valuestring);
    wstk_mutex_unlock(servlet->mutex);

    if(!service_entry) {
        js_error = rpc_create_error(RPC_ORIGIN_SERVER, RPC_ERROR_SERVICE_NOT_FOUND, rpc_service->valuestring);
        if(!js_error) { http_err_code = 500; goto out; }
    } else {
        wstk_httpd_autheticate(conn, msg, &sec_ctx);

        sentry_refs(service_entry);
        hresult = service_entry->hnadler(&sec_ctx, rpc_method->valuestring, rpc_params);
        sentry_derefs(service_entry);

        wstk_httpd_sec_ctx_clean(&sec_ctx);
    }

    cJSON_AddItemToObject(js_rsp, "id", cJSON_CreateNumber(rpc_id->valueint));
    if(hresult) {
        cJSON_AddItemToObject(js_rsp, "error",  (hresult->error && hresult->obj ? hresult->obj : cJSON_CreateNull()));
        cJSON_AddItemToObject(js_rsp, "result", (!hresult->error && hresult->obj ? hresult->obj : cJSON_CreateNull()));
    } else {
        cJSON_AddItemToObject(js_rsp, "error", (js_error ? js_error : cJSON_CreateNull()));
        cJSON_AddItemToObject(js_rsp, "result", cJSON_CreateNull());
    }

    rpc_write_response(servlet, conn, js_rsp);

out:
    if(http_err_code) {
        wstk_httpd_ereply(conn, http_err_code, http_err_msg);
    }
    if(js_req) {
        cJSON_Delete(js_req);
    }
    if(js_rsp) {
        cJSON_Delete(js_rsp);
    }

    wstk_mem_deref(buffer);
    wstk_mem_deref(hresult);
}



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create and register JSON-RPC servlet
 *
 * @param srv       - he server
 * @param path      - servlet path
 * @param servlet   - a new servlet instance
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_httpd_register_servlet_jsonrpc(wstk_httpd_t *srv, char *path, wstk_servlet_jsonrpc_t **servlet) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_servlet_jsonrpc_t *servlet_local = NULL;
    const char *charset = NULL;;

    if(!servlet || !srv || !path) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&servlet_local, sizeof(wstk_servlet_jsonrpc_t), desctuctor__wstk_servlet_jsonrpc_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&servlet_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_hash_init(&servlet_local->services)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_httpd_charset(srv, &charset)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_sdprintf(&servlet_local->ctype, JSRPC_CONTENT_TYPE_FMT, charset)) != WSTK_STATUS_SUCCESS) {
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
 * Resgister service
 *
 * @param servlet       - servlet instance
 * @param name          - service name
 * @param handler       - request handler
 * @param udata         - user data
 * @param auto_destroy  - if true will be destroyed as service unregistered
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_jsonrpc_register_service(wstk_servlet_jsonrpc_t *servlet, char *name, wstk_servlet_jsonrpc_service_handler_t handler, void *udata, bool auto_destroy) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    service_entry_t *entry = NULL;

    if(!servlet || !name || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(servlet->mutex);
    entry = wstk_hash_find(servlet->services, name);
    if(!entry) {
        status = wstk_mem_zalloc((void *)&entry, sizeof(service_entry_t), desctuctor__service_entry_t);
        if(status == WSTK_STATUS_SUCCESS) {
            entry->hnadler = handler;
            entry->name = wstk_str_dup(name);
            entry->udata = udata;
            entry->fl_adestroy_udata = auto_destroy;

            status = wstk_mutex_create(&entry->mutex);
            if(status == WSTK_STATUS_SUCCESS) {
                status = wstk_hash_insert_ex(servlet->services, name, entry, true);
            }
        }
    } else {
        status = WSTK_STATUS_ALREADY_EXISTS;
    }
    wstk_mutex_unlock(servlet->mutex);

    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(entry);
    } else {
#ifdef WSTK_SERVLET_JSONRPC_DEBUG
        WSTK_DBG_PRINT("service registered: service=%p (name=%s, udate=%p, destroy_udata=%d)", entry, entry->name, entry->udata, entry->fl_adestroy_udata);
#endif
    }

    return status;
}

/**
 * Unresgister service
 *
 * @param servlet   - servlet instance
 * @param name      - service name
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_servlet_jsonrpc_unregister_service(wstk_servlet_jsonrpc_t *servlet, char *name) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!servlet || !name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(servlet->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(servlet->mutex);
    wstk_hash_delete(servlet->services, name);
    wstk_mutex_unlock(servlet->mutex);

    return status;
}

/**
 * Make a result
 *
 * @param data  - some of json data
 *
 * @return result or NULL
 **/
wstk_servlet_jsonrpc_handler_result_t *wstk_servlet_jsonrpc_handler_result_ok(cJSON *data) {
    wstk_servlet_jsonrpc_handler_result_t *res = NULL;

    if(wstk_mem_zalloc((void *)&res, sizeof(wstk_servlet_jsonrpc_handler_result_t), NULL) == WSTK_STATUS_SUCCESS) {
        res->obj = data;
        res->error = false;
    }

    return res;
}

/**
 * Make a rpc error
 *
 * @param code      - app defiend code (should less 1000 for RPC_ORIGIN_SERVER)
 * @param message   - error message
 *
 * @return jsonrpc_handler_result_t or NULL
 **/
wstk_servlet_jsonrpc_handler_result_t *wstk_servlet_jsonrpc_handler_result_error(uint32_t code, const char *message) {
    wstk_servlet_jsonrpc_handler_result_t *res = NULL;

    if(wstk_mem_zalloc((void *)&res, sizeof(wstk_servlet_jsonrpc_handler_result_t), NULL) == WSTK_STATUS_SUCCESS) {
        res->obj = rpc_create_error((code < 1000 ? RPC_ORIGIN_SERVER : RPC_ORIGIN_SERVER), code, (char *)message);
        res->error = true;
    }

    return res;
}
