/**
 **
 ** (C)2019 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>

static bool _init = false;

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_pvt_net_init() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(_init) {
        return status;
    }

out:
    _init = true;
    return status;
}


wstk_status_t wstk_pvt_net_shutdown() {
    if(!_init) {
        return WSTK_STATUS_SUCCESS;
    }

    _init = false;
    return WSTK_STATUS_SUCCESS;
}


