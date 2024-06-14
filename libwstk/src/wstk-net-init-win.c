/**
 **
 ** (C)2024 aks
 **/
#include <wstk-net.h>
#include <wstk-log.h>

static bool _init = false;

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_pvt_net_init() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    int err = 0;

    if(_init) {
        return status;
    }

    err = WSAStartup(wVersionRequested, &wsaData);
    if(err != 0) {
        log_error("WSAStartup() err=%i", err);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    // Confirm that the WinSock DLL supports 2.2.
    // Note that if the DLL supports versions greater
    // than 2.2 in addition to 2.2, it will still return
    // 2.2 in wVersion since that is the version we requested.
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2 ) {
        WSACleanup();
        log_error("Bad winsock verion (%d.%d)\n", HIBYTE(wsaData.wVersion), LOBYTE(wsaData.wVersion));
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

out:
    _init = true;
    return status;
}


wstk_status_t wstk_pvt_net_shutdown() {
    int err = 0;

    if(!_init) {
        return WSTK_STATUS_SUCCESS;
    }

    err = WSACleanup();
    if(err) {
        log_warn("WSACleanup err=%d\n", err);
    }

    _init = false;
    return WSTK_STATUS_SUCCESS;
}


