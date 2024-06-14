/**
 **
 ** (C)2020 aks
 **/
#ifndef WSTK_HASHTABLE_H
#define WSTK_HASHTABLE_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WSTK_HASHTABLE_FLAG_NONE        = 0,
    WSTK_HASHTABLE_FLAG_FREE_KEY    = (1 << 0),
    WSTK_HASHTABLE_FLAG_FREE_VALUE  = (1 << 1),
    WSTK_HASHTABLE_DUP_CHECK        = (1 << 2)
} wstk_hashtable_flag_t;

typedef struct wstk_hashtable wstk_hash_t;
typedef struct wstk_hashtable wstk_inthash_t;
typedef struct wstk_hashtable_iterator wstk_hash_index_t;
typedef void (*wstk_hashtable_destructor_t)(void *ptr);

#define wstk_hash_init(_hash) wstk_hash_init_case(_hash, true)
#define wstk_hash_init_nocase(_hash) wstk_hash_init_case(_hash, false)
#define wstk_hash_insert(_h, _k, _d) wstk_hash_insert_destructor(_h, _k, _d, NULL)

wstk_status_t wstk_hash_init_case(wstk_hash_t **hash, bool case_sensitive);
wstk_status_t wstk_hash_insert_destructor(wstk_hash_t *hash, const char *key, const void *data, wstk_hashtable_destructor_t destructor);
wstk_status_t wstk_hash_insert_ex(wstk_hash_t *hash, const char *key, const void *data, bool destroy_val);

unsigned int wstk_hash_size(wstk_hash_t *hash);
void *wstk_hash_delete(wstk_hash_t *hash, const char *key);
void *wstk_hash_find(wstk_hash_t *hash, const char *key);
bool wstk_hash_is_empty(wstk_hash_t *hash);
wstk_hash_index_t *wstk_hash_first(wstk_hash_t *hash);
wstk_hash_index_t *wstk_hash_first_iter(wstk_hash_t *hash, wstk_hash_index_t *hi);
wstk_hash_index_t *wstk_hash_next(wstk_hash_index_t **hi);
void wstk_hash_this(wstk_hash_index_t *hi, const void **key, size_t *klen, void **val);
void wstk_hash_this_val(wstk_hash_index_t *hi, void *val);

void wstk_hash_iter_free(wstk_hash_index_t *hi);

#define wstk_inthash_insert(_h, _k, _d) wstk_inthash_insert_ex(_h, _k, _d, false)

wstk_status_t wstk_inthash_init(wstk_inthash_t **hash);
wstk_status_t wstk_inthash_insert_ex(wstk_inthash_t *hash, uint32_t key, const void *data, bool destroy_val);
void *wstk_core_inthash_delete(wstk_inthash_t *hash, uint32_t key);
void *wstk_core_inthash_find(wstk_inthash_t *hash, uint32_t key);


#ifdef __cplusplus
}
#endif
#endif
