/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_SERVLET_UPLOAD_H
#define WSTK_SERVLET_UPLOAD_H
#include <wstk-core.h>
#include <wstk-httpd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_servlet_upload_s wstk_servlet_upload_t;
typedef bool (*wstk_servlet_upload_access_handler_t)(wstk_httpd_sec_ctx_t *ctx);
typedef bool (*wstk_servlet_upload_accept_handler_t)(wstk_httpd_sec_ctx_t *ctx, char *filename, size_t size);
typedef void (*wstk_servlet_upload_complete_handler_t)(wstk_httpd_sec_ctx_t *ctx, char *path);

wstk_status_t wstk_httpd_register_servlet_upload(wstk_httpd_t *srv, char *path, wstk_servlet_upload_t **servlet);

wstk_status_t wstk_servlet_upload_configure(wstk_servlet_upload_t *servlet, char *upload_path, uint32_t max_file_size);

wstk_status_t wstk_servlet_upload_set_access_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_access_handler_t handler);
wstk_status_t wstk_servlet_upload_set_accept_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_accept_handler_t handler);
wstk_status_t wstk_servlet_upload_set_complete_handler(wstk_servlet_upload_t *servlet, wstk_servlet_upload_complete_handler_t handler);



#ifdef __cplusplus
}
#endif
#endif
