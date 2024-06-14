/**
 **
 ** (C)2018 aks
 **/
#include <wstk-mutex.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <pthread.h>

struct wstk_mutex_s {
    pthread_mutex_t hmtx;
};

static void destructor__wstk_mutex_t(void *data) {
    wstk_mutex_t *mtx = data;
    int err = 0;

    if(!mtx) { return; }

    if((err = pthread_mutex_destroy(&mtx->hmtx)) != 0) {
        log_error("Couldn't destroy mutex handler (err=%i)", err);
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
    pthread_mutexattr_t attr;
    wstk_mutex_t *mtx_local = NULL;
    int err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&mtx_local, sizeof(wstk_mutex_t), destructor__wstk_mutex_t);
    if(status !=  WSTK_STATUS_SUCCESS) { goto out; }

    pthread_mutexattr_init(&attr);

#ifdef WSTK_MUTEX_DEBUG
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#else
    /* pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL); */
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif

    if((err = pthread_mutex_init(&mtx_local->hmtx, &attr)) != 0) {
        log_error("pthread_mutex_init() failed (err=%i)", err);
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
    int err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = pthread_mutex_lock(&mtx->hmtx)) != 0) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_trylock(wstk_mutex_t *mtx) {
    int err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = pthread_mutex_trylock(&mtx->hmtx)) != 0) {
        return (err == EBUSY ? WSTK_STATUS_BUSY : WSTK_STATUS_FALSE);
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mutex_unlock(wstk_mutex_t *mtx) {
    int err;

    if(!mtx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((err = pthread_mutex_unlock(&mtx->hmtx)) != 0) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}


