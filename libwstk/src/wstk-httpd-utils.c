/**
 **
 ** (C)2024 aks
 **/
#include <wstk-httpd.h>
#include <wstk-log.h>
#include <wstk-tcp-srv.h>
#include <wstk-net.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-time.h>
#include <wstk-dir.h>

typedef struct {
    wstk_httpd_t     *httpd;
    wstk_socket_t    *sock;
} dir_browser_cb_opt_t;

static wstk_status_t xxx_dir_entry_print(wstk_dir_entry_t *dirent, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    dir_browser_cb_opt_t *opt = (dir_browser_cb_opt_t *)udata;
    wstk_socket_t *sock = opt->sock;
    char tbuf[128] = {0};

    if(!sock) {
        return WSTK_STATUS_FALSE;
    }

    if((status = wstk_time_to_str(dirent->mtime, (char *)tbuf, sizeof(tbuf))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(dirent->directory) {
        status = wstk_tcp_printf(sock, "<tr><td width=\"100%%\"><a href=\"%s/\">%s/</a></td><td>%s</td><td aling=\"right\">---</td></tr>\n", dirent->name, dirent->name, (char *)tbuf);
    } else {
        status = wstk_tcp_printf(sock, "<tr><td width=\"100%%\"><a href=\"%s\">%s</a></td><td>%s</td><td aling=\"right\">%d</td></tr>\n", dirent->name, dirent->name, (char *)tbuf, dirent->size);
    }

out:
    return status;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char *wstk_httpd_reason_by_code(uint32_t scode) {
    switch(scode) {
        case 200: return "OK";
        case 400: return "Bad request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
    }
    return "";
}
#define CTINFO(t,b) { ctype->ctype=t; ctype->binary=b; }
wstk_status_t wstk_httpd_content_type_by_file_ext(wstk_httpd_ctype_info_t *ctype, char *filename) {
    if(!ctype) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!filename) {
        CTINFO("application/octet-stream", true);
    }
    else if(wstk_strstr(filename, ".js")) {
        CTINFO("application/javascript", false);
    }
    else if(wstk_strstr(filename, ".json")) {
        CTINFO("application/json", false);
    }
    else if(wstk_strstr(filename, ".xml")) {
        CTINFO("application/xml", false);
    }
    else if(wstk_strstr(filename, ".cvs")) {
        CTINFO("text/cvs", false);
    }
    else if(wstk_strstr(filename, ".txt")) {
        CTINFO("text/plain", false);
    }
    else if(wstk_strstr(filename, ".html")) {
        CTINFO("text/html", false);
    }
    else if(wstk_strstr(filename, ".css")) {
        CTINFO("text/css", false);
    }
    else if(wstk_strstr(filename, ".png")) {
        CTINFO("image/png", true);
    }
    else if(wstk_strstr(filename, ".jpg")) {
        CTINFO("image/png", true);
    }
    else if(wstk_strstr(filename, ".gif")) {
        CTINFO("image/gif", true);
    }
    else if(wstk_strstr(filename, ".mp3")) {
        CTINFO("audio/mp3", true);
    }
    else if(wstk_strstr(filename, ".wav")) {
        CTINFO("audio/wav", true);
    } else {
        CTINFO("application/octet-stream", true);
    }
    return WSTK_STATUS_SUCCESS;
}

bool wstk_httpd_is_valid_filename(char *filename, size_t len) {
    bool result = true;
    uint32_t ofs = 0;

    if(!filename || !len) {
        return false;
    }

    for(ofs=0; ofs < len; ofs++) {
        if(filename[ofs] == '.' && filename[ofs + 1] == '.') {
            result = false; break;
        }
        if(filename[ofs] == '/' || filename[ofs] == '\\') {
            result = false; break;
        }
    }
    return result;
}

bool wstk_httpd_is_valid_path(char *path, size_t len) {
    bool result = true;
    uint32_t ofs = 0;

    if(!path || !len) {
        return false;
    }

    for(ofs=0; ofs < len; ofs++) {
        if(path[ofs] == '.' && path[ofs + 1] == '.') {
            result = false; break;
        }
        if(path[ofs] == '%') {
            result = false; break;
        }
    }
    return result;
}

/**
 * produce dir list
 *
 * @param charset - client charset
 *
 **/
wstk_status_t wstk_httpd_browse_dir(wstk_http_conn_t *conn, char *dir, char *title, char *ctype, wstk_pl_t *charset) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    dir_browser_cb_opt_t cb_opt = {0};
    wstk_socket_t *sock = NULL;
    const char *keep_alive_str;

    if(!conn || !dir) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_tcp_srv_conn_socket(conn->tcp_conn, &sock)) != WSTK_STATUS_SUCCESS) {
        return status;
    }
    if(!sock) {
        return WSTK_STATUS_FALSE;
    }

    cb_opt.httpd = conn->server;
    cb_opt.sock = sock;

    // header
    keep_alive_str = (wstk_tcp_srv_conn_is_closed(conn->tcp_conn) ? "close" : "keep-alive");
    status = wstk_httpd_reply(conn, 200, "OK",
                        "Connection: %s\r\n"
                        "Content-Type: %s\r\n"
                        "\r\n",
                        keep_alive_str,
                        ctype
            );
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    // body
    status = wstk_tcp_printf(sock, "<html>\n<head><title>Index of %s</title>\n<style>\ntable, th, td {border: 1px solid black; border-collapse: collapse; white-space: nowrap;}\n</style>\n</head>\n<body>\n", title);
    if(status != WSTK_STATUS_SUCCESS) { goto html_end; }

    status = wstk_tcp_printf(sock, "<h1>Index of %s</h1><table width=\"100%%\"><tr><th>Name</th><th>Date</th><th>Size</th></tr>\n", title);
    if(status != WSTK_STATUS_SUCCESS) { goto html_end; }

    status = wstk_tcp_printf(sock, "<tr><td colspan=\"2\"><a href=\"../\">../</a></td></tr>\n");
    if(status != WSTK_STATUS_SUCCESS) { goto html_end; }

    status = wstk_dir_list(dir, WSTK_PATH_DELIMITER, xxx_dir_entry_print, &cb_opt);
    if(status != WSTK_STATUS_SUCCESS) { goto html_end; }

html_end:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_tcp_printf(sock, "\n<br>*** ERROR ***\n<br>");
        status = WSTK_STATUS_SUCCESS;
    }
    wstk_tcp_printf(sock, "</table></body>\n</html>\n");
out:
    return status;
}


/**
 * helper to clean ctx
 *
 **/
wstk_status_t wstk_httpd_sec_ctx_clean(wstk_httpd_sec_ctx_t *sec_ctx) {
    if(!sec_ctx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(sec_ctx->user_identity && sec_ctx->destroy_identity) {
        sec_ctx->user_identity = wstk_mem_deref(sec_ctx->user_identity);
    }

    sec_ctx->token = wstk_mem_deref(sec_ctx->token);
    sec_ctx->session = wstk_mem_deref(sec_ctx->session);

    return WSTK_STATUS_SUCCESS;
}

/**
 * helper to clone ctx
 *
 **/
wstk_status_t wstk_httpd_sec_ctx_clone(wstk_httpd_sec_ctx_t **new_ctx, wstk_httpd_sec_ctx_t *sec_ctx) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_httpd_sec_ctx_t *lctx = NULL;

    if(!sec_ctx || !new_ctx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&lctx, sizeof(wstk_httpd_sec_ctx_t), NULL);
    if(status != WSTK_STATUS_SUCCESS) {
        return status;
    }

    lctx->user_identity = sec_ctx->user_identity;
    lctx->destroy_identity = sec_ctx->destroy_identity;

    lctx->session = wstk_str_dup(sec_ctx->session);
    lctx->token = wstk_str_dup(sec_ctx->token);
    lctx->peer = sec_ctx->peer;

    *new_ctx = lctx;
    return status;
}
