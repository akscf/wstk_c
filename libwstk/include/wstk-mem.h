/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_MEM_H
#define WSTK_MEM_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wstk_mem_destructor_h)(void *data);

wstk_status_t wstk_mem_alloc(void **mem, size_t size, wstk_mem_destructor_h dh);
wstk_status_t wstk_mem_zalloc(void **mem, size_t size, wstk_mem_destructor_h dh);
wstk_status_t wstk_mem_realloc(void **mem, size_t size);

wstk_status_t wstk_mem_bzero(void *mem, size_t size);

void *wstk_mem_ref(void *mem);
void *wstk_mem_deref(void *mem);
void *wstk_mem_wrap(void *cptr, size_t size, wstk_mem_destructor_h dh);



#ifdef __cplusplus
}
#endif
#endif
