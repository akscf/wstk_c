/**
 **
 ** (C)2019 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>
#include <wstk-regex.h>

static bool _init = false;

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_pvt_net_init() {
    if(_init) {
        return WSTK_STATUS_SUCCESS;
    }

    _init = true;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pvt_net_shutdown() {
    if(!_init) {
        return WSTK_STATUS_SUCCESS;
    }

    _init = false;
    return WSTK_STATUS_SUCCESS;
}


