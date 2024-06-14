/**
 ** based on apache-apr
 **
 ** (C)2018 aks
 **/
#include <wstk-mutex.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>

struct wstk_mutex_s {
    unsigned long hmtx;
};

static void destructor__wstk_mutex_t(void *data) {
    wstk_mutex_t *mtx = data;
    ULONG err;

    if(!mtx) { return; }

    if(mtx->hmtx) {
        while(DosReleaseMutexSem(mtx->hmtx) == 0) {
            /* do nothing */
        }
        if((err = DosCloseMutexSem(mtx->hmtx)) != 0) {
            log_error("DosCloseMutexSem: err=%d", err);
        }
    }

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

    if((err = DosCreateMutexSem(NULL, &(mtx_local->hmtx), 0, FALSE)) != 0) {
        log_error("DosCreateMutexSem: err=%d", err);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

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
    ULONG err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = DosRequestMutexSem(mtx->hmtx, SEM_INDEFINITE_WAIT)) != 0) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_trylock(wstk_mutex_t *mtx) {
    ULONG err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = DosRequestMutexSem(mtx->hmtx, SEM_IMMEDIATE_RETURN)) != 0) {
        return (err == ERROR_TIMEOUT ? WSTK_STATUS_BUSY : WSTK_STATUS_FALSE);
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_unlock(wstk_mutex_t *mtx) {
    ULONG err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = DosReleaseMutexSem(mtx->hmtx)) != 0) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}
