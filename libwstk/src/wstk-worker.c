/**
 ** async performing tool
 **
 ** (C)2024 aks
 **/
#include <wstk-worker.h>
#include <wstk-log.h>
#include <wstk-thread.h>
#include <wstk-sleep.h>
#include <wstk-mutex.h>
#include <wstk-queue.h>
#include <wstk-time.h>
#include <wstk-mem.h>

#define WORKER_SUB_TH_DELAY     250
#define WORKER_MAIN_TH_DELAY    250
#define WORKER_DEF_QUEUE_SIZE   128

typedef enum {
    WTF_USE_IDLE = (1<<0)
} worker_th_flags_e;

struct wstk_worker_s {
    wstk_mutex_t            *mutex;
    wstk_queue_t            *jobsq;
    wstk_worker_handler_t   handler;
    uint32_t                id;
    uint32_t                idle;
    uint32_t                refs;
    uint32_t                sub_threads;
    uint32_t                idle_threads;
    uint32_t                min_threads;
    uint32_t                max_threads;
    bool                    fl_destroyed;
    bool                    fl_ready;
};

static void worker_main_thead(wstk_thread_t *th, void *qdata);
static void worker_sub_thread(wstk_thread_t *th, void *qdata);

static wstk_status_t subth_inc(wstk_worker_t *worker) {
    if(!worker || worker->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    wstk_mutex_lock(worker->mutex);
    worker->sub_threads++;
    wstk_mutex_unlock(worker->mutex);

    return WSTK_STATUS_SUCCESS;
}
static void subth_dec(wstk_worker_t *worker) {
    if(!worker)  { return; }

    wstk_mutex_lock(worker->mutex);
    if(worker->sub_threads > 0) worker->sub_threads--;
    wstk_mutex_unlock(worker->mutex);
}
static wstk_status_t idleth_inc(wstk_worker_t *worker) {
    if(!worker || worker->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }
    wstk_mutex_lock(worker->mutex);
    worker->idle_threads++;
    wstk_mutex_unlock(worker->mutex);

    return WSTK_STATUS_SUCCESS;
}
static void idleth_dec(wstk_worker_t *worker) {
    if(!worker)  { return; }

    wstk_mutex_lock(worker->mutex);
    if(worker->idle_threads > 0) worker->idle_threads--;
    wstk_mutex_unlock(worker->mutex);
}

static wstk_status_t worker_refs(wstk_worker_t *worker) {
    if(!worker || worker->fl_destroyed)  {
        return WSTK_STATUS_FALSE;
    }

    wstk_mutex_lock(worker->mutex);
    worker->refs++;
    wstk_mutex_unlock(worker->mutex);

    return WSTK_STATUS_SUCCESS;
}
static void worker_derefs(wstk_worker_t *worker) {
    if(!worker)  { return; }

    wstk_mutex_lock(worker->mutex);
    if(worker->refs) worker->refs--;
    wstk_mutex_unlock(worker->mutex);
}

static wstk_status_t worker_sub_thread_launch(wstk_worker_t *worker, uint32_t flags) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!worker)  {
        return WSTK_STATUS_FALSE;
    }

    if((status = subth_inc(worker)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_thread_create(NULL, worker_sub_thread, worker, flags)) != WSTK_STATUS_SUCCESS) {
        subth_dec(worker);
    }
out:
    return status;
}

static void destructor__wstk_worker_t(void *data) {
    wstk_worker_t *worker = (wstk_worker_t *)data;
    bool floop = true;

    if(!worker || worker->fl_destroyed) {
        return;
    }
    worker->fl_destroyed = true;
    worker->fl_ready = false;

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("destroying worker: worker=%p (refs=%d, sub-threds=%d)", worker, worker->refs, worker->sub_threads);
#endif

    if(worker->mutex) {
        while(floop) {
            wstk_mutex_lock(worker->mutex);
            floop = (worker->refs > 0);
            wstk_mutex_unlock(worker->mutex);

            WSTK_SCHED_YIELD(0);
        }
    }

    if(worker->refs) {
        log_warn("Lost references (refs=%d, sub_threads=%d)", worker->refs, worker->sub_threads);
    }

    worker->jobsq = wstk_mem_deref(worker->jobsq);
    worker->mutex = wstk_mem_deref(worker->mutex);

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("worker destroyed: worker=%p", worker);
#endif
}

static void worker_main_thead(wstk_thread_t *th, void *qdata) {
    wstk_worker_t *worker = (wstk_worker_t *)qdata;
    wstk_status_t status = WSTK_STATUS_FALSE;
    bool fl_run_workers = false;
    bool floop = true;
    uint32_t qlen=0, herr = 0;

    worker_refs(worker);

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("mian-thread started: thread=%p ", th);
#endif

    wstk_mutex_lock(worker->mutex);
    wstk_thread_id(th, &worker->id);
    worker->fl_ready = true;
    wstk_mutex_unlock(worker->mutex);

    while(!worker->fl_destroyed) {
        /* lauch min necessary threads */
        if(worker->min_threads && worker->sub_threads < worker->min_threads) {
            for(;worker->sub_threads < worker->min_threads;) {
                if(worker->fl_destroyed) {
                    break;
                }
                if(worker_sub_thread_launch(worker, 0) != WSTK_STATUS_SUCCESS) {
                    log_error("Unable to launch sub-thread");
                    if(++herr > 10) { break; }
                }
            }
            goto timer;
        }

        /* additional threads if has something in queue */
        fl_run_workers = false;
        if(wstk_queue_len(worker->jobsq, &qlen) == WSTK_STATUS_SUCCESS) {
            if(!qlen)  { goto timer; }
            wstk_mutex_lock(worker->mutex);
            if(qlen && !worker->idle_threads) { fl_run_workers = true; }
            wstk_mutex_unlock(worker->mutex);
        }

        if(fl_run_workers && worker->sub_threads < worker->max_threads) {
            for(;;) {
                if(worker->fl_destroyed) { break; }
                if(worker->sub_threads >= worker->max_threads || qlen == 0) { break; }
                if(worker_sub_thread_launch(worker, WTF_USE_IDLE) == WSTK_STATUS_SUCCESS) {
                    qlen--;
                } else {
                    log_error("Unable to launch sub-thread");
                    break;
                }
            }
        }

        timer:
        WSTK_SCHED_YIELD(WORKER_MAIN_TH_DELAY);
    }

    if(herr) {

    }

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("main-thread stopping: thread=%p (sub_threads=%d, idle_threads=%d)", th, worker->sub_threads, worker->idle_threads);
#endif

    for(floop = true; floop;) {
        wstk_mutex_lock(worker->mutex);
        floop = (worker->sub_threads > 0);
        wstk_mutex_unlock(worker->mutex);

        WSTK_SCHED_YIELD(0);
    }

    worker_derefs(worker);

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("mian-thread finished: thread=%p ", th);
#endif

}

static void worker_sub_thread(wstk_thread_t *th, void *qdata) {
    wstk_worker_t *worker = (wstk_worker_t *)qdata;
    wstk_status_t status = WSTK_STATUS_FALSE;
    uint32_t th_flags = 0, th_id = 0;
    time_t expiry = 0;
    void *pop = NULL;
    bool fl_idle = false;

    worker_refs(worker);

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("sub-thread started: thread=%p (flags=0x%x, sub-threads=%d)", th, th_flags, worker->sub_threads);
#endif

    wstk_thread_id(th, &th_id);
    wstk_thread_uflags(th, &th_flags);

    while(!worker->fl_destroyed) {
        if(!worker->fl_ready) {
            goto timer;
        }

        while(wstk_queue_pop(worker->jobsq, &pop) == WSTK_STATUS_SUCCESS) {
            if(worker->fl_destroyed) {
               break;
            }
            if(pop) {
                if(fl_idle) { idleth_dec(worker); fl_idle = false; }
                worker->handler(worker, pop);
                expiry = 0;
            }
        }

        if(worker->fl_destroyed) {
            break;
        }

        if(expiry) {
            if(expiry <= wstk_time_epoch_now()) {
                break;
            }
        } else {
            if(th_flags & WTF_USE_IDLE) {
                expiry = (wstk_time_epoch_now() + worker->idle);
            }
        }

        timer:
        if(!fl_idle) { fl_idle = true; idleth_inc(worker); }
        WSTK_SCHED_YIELD(WORKER_SUB_TH_DELAY);
    }

    if(fl_idle) {
        idleth_dec(worker);
        fl_idle = false;
    }

    subth_dec(worker);
    worker_derefs(worker);

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("sub-thread finished: thread=%p", th);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new worker
 *
 * @param worker   - a new worker
 * @param min      - min workers amount
 * @param max      - max workers amount
 * @param qsize    - queue size
 * @param idle     - workers idle time (seconds) before terminated (by def 45sec)
 * @param handler  - function to called to process queue data
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_worker_create(wstk_worker_t **worker, uint32_t min, uint32_t max, uint32_t qsize, uint32_t idle, wstk_worker_handler_t handler) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_worker_t *worker_local = NULL;

    if(!worker || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(max < min || !max ) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!qsize) {
        qsize = WORKER_DEF_QUEUE_SIZE;
    }

    status = wstk_mem_zalloc((void *)&worker_local, sizeof(wstk_worker_t), destructor__wstk_worker_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_mutex_create(&worker_local->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_queue_create(&worker_local->jobsq, qsize)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    worker_local->idle = (idle > 0 ? idle : 45);
    worker_local->handler = handler;
    worker_local->min_threads = min;
    worker_local->max_threads = max;
    worker_local->sub_threads = 0;

    if((status = wstk_thread_create(NULL, worker_main_thead, worker_local, 0)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

#ifdef WSTK_WORKER_DEBUG
    WSTK_DBG_PRINT("worker created: worker=%p (min=%d, max=%d, qsize=%d, idle=%d, handler=%p)",
              worker_local, worker_local->min_threads, worker_local->max_threads,
              qsize, worker_local->idle, worker_local->handler
    );
#endif

    *worker = worker_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(worker_local);
    }
    return status;
}

/**
 * Add data to the queue
 * it will be processing by one of the thread
 *
 * @param worker   - a worker
 * @param data     - a certain data for this worker
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_worker_perform(wstk_worker_t *worker, void *data) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!worker || !worker->fl_ready) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(worker->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    if((status = worker_refs(worker)) == WSTK_STATUS_SUCCESS) {
        status = wstk_queue_push(worker->jobsq, data);
        worker_derefs(worker);
    }

    return status;
}

/**
 * Check ready flag
 *
 * @param worker   - a worker
 *
 * @return true/false
 **/
bool wstk_worker_is_ready(wstk_worker_t *worker) {
    if(!worker || worker->fl_destroyed) {
        return false;
    }
    return worker->fl_ready;
}
