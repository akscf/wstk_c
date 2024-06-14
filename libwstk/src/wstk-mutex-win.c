/**
 **
 ** (C)2018 aks
 **/
#include <wstk-mutex.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>

struct wstk_mutex_s {
    CRITICAL_SECTION  cs;
};

static void destructor__wstk_mutex_t(void *data) {
    wstk_mutex_t *mtx = data;
    int err = 0;

    if(!mtx) { return; }

    DeleteCriticalSection(&mtx->cs);

#ifdef WSTK_MUTEX_DEBUG
    WSTK_DBG_PRINT("mutex destroyed: %p", mtx);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_mutex_create(wstk_mutex_t **mtx) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_mutex_t *mtx_local = NULL;
    ULONG err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&mtx_local, sizeof(wstk_mutex_t), destructor__wstk_mutex_t);
    if(status !=  WSTK_STATUS_SUCCESS) { goto out; }

    InitializeCriticalSection(&mtx_local->cs);

    *mtx = mtx_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(mtx_local);
    } else {
#ifdef WSTK_MUTEX_DEBUG
        WSTK_DBG_PRINT("mutex created: %p", mtx_local);
#endif
    }
    return status;
}

wstk_status_t wstk_mutex_lock(wstk_mutex_t *mtx) {
    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    EnterCriticalSection(&mtx->cs);
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_trylock(wstk_mutex_t *mtx) {
    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(TryEnterCriticalSection(&mtx->cs) == 0) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_unlock(wstk_mutex_t *mtx) {
    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    LeaveCriticalSection(&mtx->cs);
    return WSTK_STATUS_SUCCESS;
}
