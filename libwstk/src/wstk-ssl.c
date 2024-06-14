/**
 **
 ** (C)2024 aks
 **/
#include <wstk-ssl.h>
#include <wstk-log.h>

static bool _ssl_init = false;
static bool _ssl_present = false;

wstk_status_t wstk_pvt_ssl_init() {
#ifdef WSTK_USE_SSL
    SSL_library_init();
    SSL_load_error_strings();

    _ssl_present = true;
#endif
    _ssl_init = true;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pvt_ssl_shutdown() {
    if(!_ssl_init) {
        return WSTK_STATUS_SUCCESS;
    }

#ifdef WSTK_USE_SSL
    ERR_free_strings();
#endif

    return WSTK_STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool wstk_ssl_is_supported() {
    return _ssl_present;
}

//
// todo
//

