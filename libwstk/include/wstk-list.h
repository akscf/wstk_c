/**
 **
 ** (C)2020 aks
 **/
#ifndef WSTK_LIST_H
#define WSTK_LIST_H
#include <wstk-core.h>
#include <wstk-mem.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_list_s  wstk_list_t;
typedef struct {
    void        *data;
    void        *item;
    uint32_t    pos;
    bool        found;
} wstk_list_find_t;

#define wstk_list_add_head(l, data, dh) wstk_list_add(l, 0, data, dh)
#define wstk_list_add_tail(l, data, dh) wstk_list_add(l, 0xffffffff, data, dh)

wstk_status_t wstk_list_create(wstk_list_t **list);
wstk_status_t wstk_list_add(wstk_list_t *list, uint32_t pos, void *data, wstk_mem_destructor_h dh);

wstk_status_t wstk_list_clear(wstk_list_t *list, void (*callback)(uint32_t, void *, void *), void *udata);
wstk_status_t wstk_list_foreach(wstk_list_t *list, void (*callback)(uint32_t, void *, void *), void *udata);

wstk_list_find_t wstk_list_find(wstk_list_t *list, bool (*callback)(uint32_t, void *));
void *wstk_list_get(wstk_list_t *list, uint32_t pos);
void *wstk_list_del(wstk_list_t *list, uint32_t pos);

uint32_t wstk_list_get_size(wstk_list_t *list);
bool wstk_list_is_empty(wstk_list_t *list);


#ifdef __cplusplus
}
#endif
#endif
