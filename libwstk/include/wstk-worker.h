/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_WORKER_H
#define WSTK_WORKER_H
#include <wstk-core.h>
#include <wstk-thread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_worker_s  wstk_worker_t;
typedef void (*wstk_worker_handler_t)(wstk_worker_t *worker, void *qdata);

wstk_status_t wstk_worker_create(wstk_worker_t **worker, uint32_t min, uint32_t max, uint32_t qsize, uint32_t idle, wstk_worker_handler_t handler);
wstk_status_t wstk_worker_perform(wstk_worker_t *worker, void *data);
bool wstk_worker_is_ready(wstk_worker_t *worker);


#ifdef __cplusplus
}
#endif
#endif
