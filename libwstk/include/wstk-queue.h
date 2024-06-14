/**
 ** thread-safe queue
 **
 ** (C)2020 aks
 **/
#ifndef WSTK_QUEUE_H
#define WSTK_QUEUE_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_queue_s  wstk_queue_t;

wstk_status_t wstk_queue_create(wstk_queue_t **queue, uint32_t size);

bool wstk_queue_is_empty(wstk_queue_t *queue);
bool wstk_queue_is_full(wstk_queue_t *queue);

wstk_status_t wstk_queue_push(wstk_queue_t *queue, void *data);
wstk_status_t wstk_queue_pop(wstk_queue_t *queue, void **data);
wstk_status_t wstk_queue_len(wstk_queue_t *queue, uint32_t *len);



#ifdef __cplusplus
}
#endif
#endif
