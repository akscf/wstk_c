/**
 **
 ** (C)2020 aks
 **/
#include <wstk-queue.h>
#include <wstk-log.h>
#include <wstk-mutex.h>
#include <wstk-mem.h>

struct wstk_queue_s {
    wstk_mutex_t    *mutex;
    void            **data;
    uint32_t        size;
    uint32_t        in;
    uint32_t        out;
    bool            fl_destroyed;
};

static void destructor__wstk_queue_t(void *data) {
    wstk_queue_t *queue = (wstk_queue_t *)data;

    if(!queue || queue->fl_destroyed) {
        return;
    }
    queue->fl_destroyed = true;

#ifdef WSTK_QUEUE_DEBUG
    WSTK_DBG_PRINT("destroying queue: queue=%p (len=%d)", queue, queue->in);
#endif

    wstk_mutex_lock(queue->mutex);
    if(queue->in) {
        for(uint32_t i = 0; i <= queue->in; i++) {
            void *dp = queue->data[i];
            if(dp) { wstk_mem_deref(dp); }
        }
    }
    wstk_mutex_unlock(queue->mutex);

    queue->data = wstk_mem_deref(queue->data);
    queue->mutex = wstk_mem_deref(queue->mutex);

#ifdef WSTK_QUEUE_DEBUG
    WSTK_DBG_PRINT("queue destroyed: queue=%p", queue);
#endif

}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new queue
 *
 * @param queue - a new queue
 * @param size  - the size
 *
 * @return success ot some error
 **/
wstk_status_t wstk_queue_create(wstk_queue_t **queue, uint32_t size) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_queue_t *qlocal = NULL;

    if(!queue || !size) {
        return WSTK_STATUS_FALSE;
    }

    status = wstk_mem_zalloc((void *)&qlocal, sizeof(wstk_queue_t), destructor__wstk_queue_t);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if((status = wstk_mutex_create(&qlocal->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_mem_zalloc((void *)&qlocal->data, sizeof(void *) * (size + 1), NULL);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    qlocal->in = 0;
    qlocal->out = 0;
    qlocal->size = size;

#ifdef WSTK_QUEUE_DEBUG
    WSTK_DBG_PRINT("queue created: queue=%p (size=%d)", queue, qlocal->size);
#endif

    *queue = qlocal;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(qlocal);
        *queue = NULL;
    }
    return status;
}

/**
 * Queue is empty
 *
 * @param queue - the queue
 *
 * @return true / false
 **/
bool wstk_queue_is_empty(wstk_queue_t *queue) {
    bool result = false;

    if(!queue || queue->fl_destroyed) {
        return false;
    }

    wstk_mutex_lock(queue->mutex);
    result = (queue->in == 0);
    wstk_mutex_unlock(queue->mutex);

    return result;
}

/**
 * Queue is full
 *
 * @param queue - the queue
 *
 * @return true / false
 **/
bool wstk_queue_is_full(wstk_queue_t *queue) {
    bool result = false;

    if(!queue || queue->fl_destroyed) {
        return false;
    }

    wstk_mutex_lock(queue->mutex);
    result = (queue->in >= queue->size);
    wstk_mutex_unlock(queue->mutex);

    return result;
}

/**
 * Push data to queue
 *
 * @param queue - the queue
 * @param data  - the dadata
 *
 * @return success ot some error
 **/
wstk_status_t wstk_queue_push(wstk_queue_t *queue, void *data) {
    wstk_status_t status = WSTK_STATUS_NOSPACE;

    if(!queue || !data) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(queue->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(queue->mutex);
    if(queue->in < queue->size) {
        queue->data[queue->in] = data;
        queue->in++;

        status = WSTK_STATUS_SUCCESS;
    }
    wstk_mutex_unlock(queue->mutex);

    return status;
}

/**
 * Pop data from queue
 *
 * @param queue - the queue
 * @param data  - the dadata
 *
 * @return success ot some error
 **/
wstk_status_t wstk_queue_pop(wstk_queue_t *queue, void **data) {
    wstk_status_t status = WSTK_STATUS_NODATA;

    if(!queue || !data) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(queue->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(queue->mutex);
    if(queue->in > 0) {
        *data = queue->data[queue->out];

        queue->data[queue->out] = NULL;
        queue->out++;

        if(queue->out > queue->in || queue->out >= queue->size) {
            queue->in = 0;
            queue->out = 0;
        }

        status = WSTK_STATUS_SUCCESS;
    }
    wstk_mutex_unlock(queue->mutex);

    return status;
}

/**
 * Get current queue length
 *
 * @param queue - the queue
 * @param len   - curr len
 *
 * @return success ot some error
 **/
wstk_status_t wstk_queue_len(wstk_queue_t *queue, uint32_t *len) {
    if(!queue || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(queue->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(queue->mutex);
    *len = queue->in;
    wstk_mutex_unlock(queue->mutex);

    return WSTK_STATUS_SUCCESS;
}
