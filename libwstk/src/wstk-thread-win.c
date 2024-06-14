/**
 **
 ** (C)2019 aks
 **/
#include <wstk-thread.h>
#include <wstk-log.h>
#include <wstk-mutex.h>
#include <wstk-mem.h>
#include <wstk-log.h>

struct wstk_thread_s {
    HANDLE              td;
    wstk_thread_func_t  func;
    wstk_mutex_t        *mutex;
    void                *udata;
    uint32_t            tid;
    uint32_t            uflags;
    uint32_t            cancel_req;
    bool                fl_alive;
    bool                fl_destroyed;
};

static void destructor__wstk_thread_t(void *data) {
    wstk_thread_t *th = (wstk_thread_t *)data;

    if(!th || th->fl_destroyed) { return; }
    th->fl_destroyed = true;

    if(th->td) {
        CloseHandle(th->td);
    }

    th->mutex = wstk_mem_deref(th->mutex);

#ifdef WSTK_THREAD_DEBUG
    WSTK_DBG_PRINT("thread destroyed: %p", th);
#endif
}

static DWORD WINAPI xxx_thread_proc(void *arg){
    wstk_thread_t *th = (wstk_thread_t *)arg;

    if(th && !th->fl_destroyed) {
        th->func(th, th->udata);
        th->fl_alive = false;
    }

    wstk_mem_deref(th);
    ExitThread(0);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_thread_yield() {
    Sleep(0);
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_thread_create(wstk_thread_t **thread, wstk_thread_func_t func, void *udata, uint32_t uflags) {
    uint32_t stack_size = 0;

#ifdef WSTK_THREAD_STACK_SIZE
    stack_size = WSTK_THREAD_STACK_SIZE;
#endif

    return wstk_thread_create2(thread, func, stack_size, udata, uflags);
}

wstk_status_t wstk_thread_create2(wstk_thread_t **thread, wstk_thread_func_t func, uint32_t stack_size, void *udata, uint32_t uflags) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_thread_t *th = NULL;

    int err;

    if(!func) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&th, sizeof(wstk_thread_t), destructor__wstk_thread_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_mutex_create(&th->mutex);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    th->func = func;
    th->udata = udata;
    th->uflags = uflags;
    th->fl_alive = true;

    th->td = CreateThread(NULL, stack_size, xxx_thread_proc, th, 0, (LPDWORD)&th->tid);
    if(!th->td) {
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(thread) {
        *thread = th;
        wstk_mem_ref(th);
    }

#ifdef WSTK_THREAD_DEBUG
    WSTK_DBG_PRINT("thread created: thread=%p (stack-size=%d)", th, stack_size);
#endif

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(th);
    }
    return status;
}

wstk_status_t wstk_thread_join(wstk_thread_t *th) {
    DWORD rc = 0;

    if(!th) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(!th->fl_alive || th->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if(!th->td) {
        return WSTK_STATUS_FALSE;
    }

    rc = WaitForSingleObject(th->td, INFINITE);
    if(rc == WAIT_OBJECT_0 || rc == WAIT_ABANDONED) {
        return WSTK_STATUS_SUCCESS;
    }

    CloseHandle(th->td);
    th->td = NULL;

    return WSTK_STATUS_FALSE;
}

wstk_status_t wstk_thread_try_cancel(wstk_thread_t *th) {
    if(!th) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(!th->fl_alive || th->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(th->mutex);
    th->cancel_req++;
    wstk_mutex_unlock(th->mutex);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_thread_id(wstk_thread_t *th, uint32_t *id) {
    if(!th) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(th->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    *id = (uint32_t)th->tid;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_thread_uflags(wstk_thread_t *th, uint32_t *flags) {
    if(!th) {
        return WSTK_STATUS_FALSE;
    }
    if(th->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(th->mutex);
    *flags = th->uflags;
    wstk_mutex_unlock(th->mutex);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_thread_set_uflags(wstk_thread_t *th, uint32_t flags) {
    if(!th) {
        return WSTK_STATUS_FALSE;
    }
    if(th->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(th->mutex);
    th->uflags = flags;
    wstk_mutex_unlock(th->mutex);

    return WSTK_STATUS_SUCCESS;
}

bool wstk_thread_is_finished(wstk_thread_t *th) {
    if(!th) {
        return false;
    }

    return (th->fl_destroyed || !th->fl_alive);
}

bool wstk_thread_is_canceled(wstk_thread_t *th) {
    bool result = false;

    if(!th || th->fl_destroyed) {
        return false;
    }

    return (th->cancel_req > 0);
}
