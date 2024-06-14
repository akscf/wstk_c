/**
 * based on libre
 *
 * (C)2019 aks
 **/
#ifndef WSTK_URI_H
#define WSTK_URI_H
#include <wstk-core.h>
#include <wstk-mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_uri_s {
    wstk_pl_t   scheme;     /** URI scheme e.g. "sip:" "sips:"    */
    wstk_pl_t   user;       /** Username                          */
    wstk_pl_t   password;   /** Optional password                 */
    wstk_pl_t   host;       /** Hostname or IP-address            */
    int         af;         /** Address family of host IP-address */
    uint16_t    port;       /** Port number                       */
} wstk_uri_t;

wstk_status_t wstk_uri_encode(wstk_uri_t *uri, char **str);
wstk_status_t wstk_uri_encode_mbuf(wstk_uri_t *uri, wstk_mbuf_t *mbuf);
wstk_status_t wstk_uri_decode(wstk_uri_t *uri, const char *str, size_t str_len);


#ifdef __cplusplus
}
#endif
#endif
