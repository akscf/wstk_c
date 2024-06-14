/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_MUTEX_H
#define WSTK_MUTEX_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_mutex_s wstk_mutex_t;

wstk_status_t wstk_mutex_create(wstk_mutex_t **mtx);
wstk_status_t wstk_mutex_lock(wstk_mutex_t *mtx);
wstk_status_t wstk_mutex_trylock(wstk_mutex_t *mtx);
wstk_status_t wstk_mutex_unlock(wstk_mutex_t *mtx);



#ifdef __cplusplus
}
#endif
#endif
