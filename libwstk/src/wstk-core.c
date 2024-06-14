/**
 **
 ** (C)2024 aks
 **/
#include <wstk.h>

extern wstk_status_t wstk_pvt_log_init();
extern wstk_status_t wstk_pvt_log_shutdown();
extern wstk_status_t wstk_pvt_rand_init();
extern wstk_status_t wstk_pvt_rand_shutdown();
extern wstk_status_t wstk_pvt_time_init();
extern wstk_status_t wstk_pvt_time_shutdown();
extern wstk_status_t wstk_pvt_net_init();
extern wstk_status_t wstk_pvt_net_shutdown();
extern wstk_status_t wstk_pvt_codepage_init();
extern wstk_status_t wstk_pvt_ssl_init();
extern wstk_status_t wstk_pvt_ssl_shutdown();

static bool core_init;
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_core_init() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(core_init) {
        return WSTK_STATUS_SUCCESS;
    }

    if((status = wstk_pvt_log_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_pvt_rand_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_pvt_time_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_pvt_net_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_pvt_codepage_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_pvt_ssl_init()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    core_init = true;
out:
    return status;
}

wstk_status_t wstk_core_shutdown() {
    if(core_init) {
        wstk_pvt_rand_shutdown();
        wstk_pvt_time_shutdown();
        wstk_pvt_net_shutdown();
        wstk_pvt_ssl_shutdown();
        wstk_pvt_log_shutdown();
    }

    return WSTK_STATUS_SUCCESS;
}
