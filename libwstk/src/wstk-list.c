/**
 ** simple linked-list
 **
 ** (C)2020 aks
 **/
#include <wstk-list.h>
#include <wstk-log.h>
#include <wstk-mem.h>

typedef struct wstk_list_item_s {
    wstk_mem_destructor_h   dh;
    void                    *data;
    struct wstk_list_item_s *next;
    struct wstk_list_item_s *prev;
} wstk_list_item_t;

struct wstk_list_s {
    uint32_t                size;
    wstk_list_item_t        *head;
    wstk_list_item_t        *tail;
};

static void destructor__wstk_list_t(void *data) {
    wstk_list_t *list = (wstk_list_t *)data;
    wstk_list_item_t *t = NULL, *tt = NULL;

#ifdef WSTK_LIST_DEBUG
    WSTK_DBG_PRINT("destroying list: list=%p (size=%d)", list, list->size);
#endif

    t = list->head;
    while(t != NULL) {
        tt = t; t = t->next;
        tt = wstk_mem_deref(tt);
    }

#ifdef WSTK_LIST_DEBUG
    WSTK_DBG_PRINT("list destroyed: list=%p", list);
#endif
}

static void destructor__wstk_list_item_t(void *data) {
    wstk_list_item_t *item = (wstk_list_item_t *)data;

    if(!item) { return; }

    if(item->dh) {
        item->dh(item->data);
        item->data = NULL;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Create a new list
 *
 * @param list  - the list
 *
 * @return success or error
 **/
wstk_status_t wstk_list_create(wstk_list_t **list) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_list_t *list_local = NULL;

    status = wstk_mem_zalloc((void *)&list_local, sizeof(wstk_list_t), destructor__wstk_list_t);
    if(status != WSTK_STATUS_SUCCESS)  {
        goto out;
    }

    *list = list_local;

#ifdef WSTK_LIST_DEBUG
    WSTK_DBG_PRINT("list created: list=%p", list);
#endif

out:
    return status;
}

/**
 * Insert item
 *
 * @param list  - the list
 * @param pos   - 0 = head, >list-size = tail, or certain posiotion
 * @param data  - some data
 * @param dh    - destroy handler
 *
 * @return success or error
 **/
wstk_status_t wstk_list_add(wstk_list_t *list, uint32_t pos, void *data, wstk_mem_destructor_h dh) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_list_item_t *item = NULL, *target = NULL;

    if(!list || !data) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(list->size == 0) {
        if((status = wstk_mem_zalloc((void *)&item, sizeof(wstk_list_item_t), destructor__wstk_list_item_t)) != WSTK_STATUS_SUCCESS) {
            return status;
        }

        item->dh = dh;
        item->data = data;
        item->next = NULL;
        item->prev = NULL;
        list->head = item;
        list->tail = item;
        list->size = 1;

        return WSTK_STATUS_SUCCESS;
    }

    /* head */
    if(pos == 0) {
        if((status = wstk_mem_zalloc((void *)&item, sizeof(wstk_list_item_t), destructor__wstk_list_item_t)) != WSTK_STATUS_SUCCESS) {
            return status;
        }

        item->dh = dh;
        item->data = data;
        item->next = list->head;
        list->head->prev = item;
        list->head = item;
        list->size = (list->size + 1);

        return WSTK_STATUS_SUCCESS;
    }

    /* tail */
    if(pos >= list->size) {
        if((status = wstk_mem_zalloc((void *)&item, sizeof(wstk_list_item_t), destructor__wstk_list_item_t)) != WSTK_STATUS_SUCCESS) {
            return status;
        }

        item->dh = dh;
        item->data = data;
        item->next = NULL;

        list->tail->next = item;
        item->prev = list->tail;
        list->tail = item;
        list->size = (list->size + 1);

        return WSTK_STATUS_SUCCESS;
    }

    /* certain position */
    target = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        if(pos == i) break;
        target = target->next;
    }
    if(!target) {
        return WSTK_STATUS_FALSE;
    }

    if((status = wstk_mem_zalloc((void *)&item, sizeof(wstk_list_item_t), destructor__wstk_list_item_t)) != WSTK_STATUS_SUCCESS) {
        return status;
    }

    item->dh = dh;
    item->data = data;
    item->next = target;
    item->prev = target->prev;
    target->prev->next = item;
    target->prev = item;

    list->size = (list->size + 1);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Delete item
 *
 * @param list  - the list
 * @param pod   - the position
 *
 * @return data or NULL
 **/
void *wstk_list_del(wstk_list_t *list, uint32_t pos) {
    wstk_list_item_t *item = NULL;
    void *data = NULL;

    if(!list || pos < 0) {
        return NULL;
    }
    if(!list->size || pos >= list->size) {
        return NULL;
    }

    if(pos == 0) {
        item = list->head;
        list->head = item->next;
        list->head->prev = NULL;

        data = (item->dh ? NULL : item->data);
        item = wstk_mem_deref(item);

        list->size = (list->size - 1);
        if(!list->size) {
            list->tail = NULL;
            list->head = NULL;
        }

        return data;
    }

    item = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        if(i == pos) break;
        item = item->next;
    }
    if(!item) {
        return NULL;
    }

    if(item == list->tail) {
        list->tail = item->prev;
        list->tail->next = NULL;
    } else {
        if(item->prev) { item->prev->next = item->next; }
        if(item->next) { item->next->prev = item->prev; }
    }

    data = (item->dh ? NULL : item->data);
    wstk_mem_deref(item);

    list->size = (list->size - 1);
    if(!list->size) {
        list->tail = NULL;
        list->head = NULL;
    }

    return data;
}

/**
 * Get item
 *
 * @param list  - the list
 * @param pod   - the position
 *
 * @return success or error
 **/
void *wstk_list_get(wstk_list_t *list, uint32_t pos) {
    wstk_list_item_t *item = NULL;

    if(!list || pos < 0) {
        return NULL;
    }

    if(list->size == 0 || pos >= list->size) {
        return NULL;
    }

    item = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        if(i == pos) break;
        item = item->next;
    }

    return(item ? item->data : NULL);
}

/**
 * Foreach list
 * callback will be called for each item
 *
 * @param list      - the list
 * @param callback  - callback (pos, data, udata)
 * @param udata     - user date
 *
 * @return success or error
 **/
wstk_status_t wstk_list_foreach(wstk_list_t *list, void (*callback)(uint32_t, void *, void *), void *udata) {
    wstk_list_item_t *target = NULL;

    if(!list || !callback) {
        return WSTK_STATUS_FALSE;
    }
    if(list->size == 0) {
        return WSTK_STATUS_SUCCESS;
    }

    target = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        if(!target) { break; }
        callback(i, target->data, udata);
        target = target->next;
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Clear list
 * callback will be called before the data be destroyed
 *
 * @param list      - the list
 * @param callback  - callback (pos, data, udata) or NULL
 * @param udata     - user date
 *
 * @return success or error
 **/
wstk_status_t wstk_list_clear(wstk_list_t *list, void (*callback)(uint32_t, void *, void *), void *udata) {
    wstk_list_item_t *next = NULL;
    wstk_list_item_t *curr = NULL;

    if(!list) {
        return WSTK_STATUS_FALSE;
    }
    if(list->size == 0) {
        return WSTK_STATUS_SUCCESS;
    }

    curr = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        next = curr->next;
        if(callback) { callback(i, curr->data, udata); }
        curr = wstk_mem_deref(curr);

        if(!next) { break; }
        curr = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;

    return WSTK_STATUS_SUCCESS;
}

/**
 * Find a certain item
 * callback uses as a compare function
 *
 * @param list      - the list
 * @param callback  - callback (pos, data)
 *
 * @return wstk_list_find_t
 **/
wstk_list_find_t wstk_list_find(wstk_list_t *list, bool (*callback)(uint32_t, void *)) {
    wstk_list_item_t *target = NULL;
    wstk_list_find_t result = { NULL, NULL, 0, false };

    if(!list || !callback) {
        return result;
    }
    if(list->size == 0) {
        return result;
    }

    target = list->head;
    for(uint32_t i = 0; i < list->size; i++) {
        if(!target) { break; }
        if(callback(i, target->data)) {
            result.pos = i;
            result.item = target;
            result.data = target->data;
            result.found = true;
            break;
        }
        target = target->next;
    }

    return result;
}

/**
 * List size
 *
 * @param list  - the list
 *
 * @return the size
 **/
uint32_t wstk_list_get_size(wstk_list_t *list) {
    if(!list) {
        return 0;
    }
    return list->size;
}

/**
 * Check on empty
 *
 * @param list  - the list
 *
 * @return true/false
 **/
bool wstk_list_is_empty(wstk_list_t *list) {
    if(!list) {
        return false;
    }
    return (list->size > 0 ? false : true);
}

