/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_HTTPD_H
#define WSTK_HTTPD_H
#include <wstk-core.h>
#include <wstk-tcp-srv.h>
#include <wstk-http-msg.h>
#include <wstk-hashtable.h>
#include <wstk-pl.h>
#include <wstk-mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_httpd_s wstk_httpd_t;

typedef struct {
    wstk_httpd_t            *server;            // refs to httpd instance
    wstk_tcp_srv_conn_t     *tcp_conn;          // refs to the tcp connection
    wstk_mbuf_t             *buffer;            // refs to the tcp connection internal buffer
    uint32_t                conn_id;            // connection id (the same as tcp_conn_id)
    bool                    websock;            // true if a websocket connection
    bool                    tls;                // true if a secure connection
} wstk_http_conn_t;

typedef struct {
    const wstk_sockaddr_t   *peer;              // peer address (refs to conn->peer)
    char                    *session;           // session id   (cloned header-attr X-SESSION-ID)
    char                    *token;             // bearer token (cloned Bearer token)
    void                    *user_identity;     // anything or NULL
    bool                    destroy_identity;   // will be destroyed after call
    bool                    permitted;          // true if access is permitted
    uint32_t                user_id;            // some id
    uint32_t                role;               // the role identifier if available
} wstk_httpd_sec_ctx_t;

typedef struct {
    wstk_http_conn_t        *conn;              // refs to conn
    wstk_http_msg_t         *msg;               // refs tp msg
    const char              *session;           // session id
    const char              *token;             // security token
    const char              *login;             // if present
    const char              *password;          // if present
    const wstk_sockaddr_t   *peer;              // peer address
} wstk_httpd_auth_request_t;

typedef struct {
    void                    *user_identity;     // anything
    bool                    destroy_identity;   // will be destroyed after call
    bool                    permitted;          // true if access is permitted
    uint32_t                user_id;            // some id
    uint32_t                role;               // the role identifier if available
} wstk_httpd_auth_response_t;

typedef wstk_status_t (*wstk_httpd_blob_reader_callback_t)(void *udata, wstk_mbuf_t *buf);
typedef void (*wstk_httpd_servlet_handler_t)(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata);
typedef void (*wstk_httpd_authentication_handler_t)(wstk_httpd_auth_request_t *req, wstk_httpd_auth_response_t *rsp);

wstk_status_t wstk_httpd_create(wstk_httpd_t **srv, wstk_sockaddr_t *address, uint32_t max_conns, uint32_t max_idle, char *charset, char *www_home, char *welcome_page, bool allow_dir_browse);
wstk_status_t wstk_httpsd_create(wstk_httpd_t **srv, wstk_sockaddr_t *address, char *cert, uint32_t max_conns, uint32_t max_idle, char *charset, char *www_home, char *welcome_page, bool allow_dir_browse);

wstk_status_t wstk_httpd_start(wstk_httpd_t *srv);
bool wstk_httpd_is_tls(wstk_httpd_t *srv);
bool wstk_httpd_is_destroyed(wstk_httpd_t *srv);

wstk_status_t wstk_httpd_register_servlet(wstk_httpd_t *srv, const char *path, wstk_httpd_servlet_handler_t handler, void *udata, bool auto_destroy);
wstk_status_t wstk_httpd_unregister_servlet(wstk_httpd_t *srv, const char *path);

wstk_status_t wstk_httpd_charset(wstk_httpd_t *srv, const char **charset);
wstk_status_t wstk_httpd_listen_address(wstk_httpd_t *srv, wstk_sockaddr_t **laddr);
wstk_status_t wstk_httpd_set_ident(wstk_httpd_t *srv, const char *server_name);
wstk_status_t wstk_httpd_set_authenticator(wstk_httpd_t *srv, wstk_httpd_authentication_handler_t handler, bool replace);
wstk_status_t wstk_httpd_autheticate(wstk_http_conn_t *conn, wstk_http_msg_t *msg, wstk_httpd_sec_ctx_t *ctx);

wstk_status_t wstk_httpd_reply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *fmt, ...);
wstk_status_t wstk_httpd_creply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *ctype, const char *fmt, ...);
wstk_status_t wstk_httpd_ereply(wstk_http_conn_t *conn, uint32_t scode, const char *reason);
wstk_status_t wstk_httpd_breply(wstk_http_conn_t *conn, uint32_t scode, const char *reason, const char *ctype, size_t blen, time_t mtime, wstk_httpd_blob_reader_callback_t rcallback, void *udata);

wstk_http_conn_t *wstk_httpd_conn_lookup(wstk_httpd_t *srv, uint32_t id);
wstk_status_t wstk_httpd_conn_take(wstk_http_conn_t *conn);
wstk_status_t wstk_httpd_conn_release(wstk_http_conn_t *conn);

wstk_status_t wstk_httpd_read(wstk_http_conn_t *conn, wstk_mbuf_t *mbuf, uint32_t timeout);
wstk_status_t wstk_httpd_conn_rdlock(wstk_http_conn_t *conn, bool flag);

/* wstk-httpd-utils.c */
typedef struct {
    const char *ctype;
    bool        binary;
} wstk_httpd_ctype_info_t;

wstk_status_t wstk_httpd_content_type_by_file_ext(wstk_httpd_ctype_info_t *ctype, char *filename);
const char *wstk_httpd_reason_by_code(uint32_t scode);

bool wstk_httpd_is_valid_path(char *path, size_t len);
bool wstk_httpd_is_valid_filename(char *filename, size_t len);
wstk_status_t wstk_httpd_browse_dir(wstk_http_conn_t *conn, char *dir, char *title, char *ctype, wstk_pl_t *charset);

wstk_status_t wstk_httpd_sec_ctx_clean(wstk_httpd_sec_ctx_t *sec_ctx);
wstk_status_t wstk_httpd_sec_ctx_clone(wstk_httpd_sec_ctx_t **new_ctx, wstk_httpd_sec_ctx_t *sec_ctx);



#ifdef __cplusplus
}
#endif
#endif
