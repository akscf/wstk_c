/**
 **
 ** (C)2020 aks
 **/
#include <wstk-hashtable.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>

/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
  Credit for primes table: Aaron Krowne
  http://br.endernet.org/~akrowne/
  http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */
struct wstk_hashtable_entry {
    void                        *k, *v;
    unsigned int                 h;
    wstk_hashtable_flag_t        flags;
    wstk_hashtable_destructor_t  destructor;
    struct wstk_hashtable_entry  *next;
};
struct wstk_hashtable_iterator {
    unsigned int                pos;
    struct wstk_hashtable_entry  *e;
    struct wstk_hashtable        *h;
};
struct wstk_hashtable {
    unsigned int                tablelength;
    struct wstk_hashtable_entry  **table;
    unsigned int                entrycount;
    unsigned int                loadlimit;
    unsigned int                primeindex;
    unsigned int (*hashfn) (void *k);
    int (*eqfn) (void *k1, void *k2);
};

static const unsigned int primes[] = {
    53, 97, 193, 389,
    769, 1543, 3079, 6151,
    12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869,
    3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189,
    805306457, 1610612741
};
const unsigned int prime_table_length = (sizeof (primes) / sizeof (primes[0]));
const float max_load_factor = 0.65f;
typedef struct wstk_hashtable wstk_hashtable_t;
typedef struct wstk_hashtable_iterator wstk_hashtable_iterator_t;

#define ht__assert(x) assert(x)

static inline unsigned int hash(struct wstk_hashtable *h, void *k) {
    /* Aim to protect against poor hash functions by adding logic here - logic taken from java 1.4 hashtable source */
    unsigned int i = h->hashfn(k);
    i += ~(i << 9);
    i ^= ((i >> 14) | (i << 18)); /* >>> */
    i += (i << 4);
    i ^= ((i >> 10) | (i << 22)); /* >>> */
    return i;
}

static __inline__ unsigned int indexFor(unsigned int tablelength, unsigned int hashvalue) {
    return (hashvalue % tablelength);
}


/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii
   http://www.azillionmonkeys.com/qed/asmexample.html
 */
static uint32_t c_tolower(uint32_t eax) {
    uint32_t ebx = (0x7f7f7f7ful & eax) + 0x25252525ul;
    ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
    ebx = ((ebx & ~eax) >> 2) & 0x20202020ul;
    return eax + ebx;
}

static void desctuctor__wstk_hashtable_t(void *ptr) {
    struct wstk_hashtable *ht = (struct wstk_hashtable *)ptr;
    struct wstk_hashtable_entry **table = ht->table;
    struct wstk_hashtable_entry *e=NULL, *f=NULL;
    uint32_t i = 0;

#ifdef WSTK_HASTABLE_DEBUG
    WSTK_DBG_PRINT("destroying hastable: htable=%p (tablelength=%d)", ht, ht->tablelength);
#endif

    for(i = 0; i < ht->tablelength; i++) {
        e = table[i];
        while(NULL != e) {
            f = e;
            e = e->next;

            if(f->flags & WSTK_HASHTABLE_FLAG_FREE_KEY) {
                f->k = wstk_mem_deref(f->k);
            }

            if(f->flags & WSTK_HASHTABLE_FLAG_FREE_VALUE) {
                f->v = wstk_mem_deref(f->v);
            } else if (f->destructor) {
                f->destructor(f->v);
                f->v = NULL;
            }

            f = wstk_mem_deref(f);
        }
    }

    ht->table = wstk_mem_deref(ht->table);

#ifdef WSTK_HASTABLE_DEBUG
    WSTK_DBG_PRINT("hastable destroyed: htable=%p", ht);
#endif
}

// ----------------------------------------------------------------------------------------------------------------------------------------------
/**
 **
 **/
wstk_status_t wstk_hashtable_create(wstk_hashtable_t **hp, unsigned int minsize, unsigned int (*hashf) (void*), int (*eqf) (void*, void*)) {
    unsigned int pindex = 0, size = primes[0];
    wstk_hashtable_t *h = NULL;

    /* Check requested hashtable isn't too large */
    if(minsize > (1u << 30)) {
        *hp = NULL;
        return WSTK_STATUS_FALSE;
    }

    /* Enforce size as prime */
    for(pindex = 0; pindex < prime_table_length; pindex++) {
        if(primes[pindex] > minsize) {
            size = primes[pindex];
            break;
        }
    }

    if(wstk_mem_zalloc((void *)&h, sizeof(wstk_hashtable_t), desctuctor__wstk_hashtable_t) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_alloc()");
        return WSTK_STATUS_MEM_FAIL;
    }
    if(wstk_mem_zalloc((void *)&h->table, (size * sizeof(struct wstk_hashtable_entry *)), NULL) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_alloc()");
        wstk_mem_deref(h);
        return WSTK_STATUS_MEM_FAIL;
    }

    h->tablelength = size;
    h->primeindex = pindex;
    h->entrycount = 0;
    h->hashfn = hashf;
    h->eqfn = eqf;
    h->loadlimit = (unsigned int) ceil(size * max_load_factor);

    *hp = h;

#ifdef WSTK_HASTABLE_DEBUG
    WSTK_DBG_PRINT("hastable created: htable=%p (tablelength=%d)", h, size);
#endif

    return WSTK_STATUS_SUCCESS;
}

/**
 **
 **/
static int hashtable_expand(wstk_hashtable_t *h) {
    /* Double the size of the table to accommodate more entries */
    struct wstk_hashtable_entry **newtable = NULL;
    struct wstk_hashtable_entry *e = NULL;
    struct wstk_hashtable_entry **pE = NULL;
    unsigned int newsize, i, index;

    /* Check we're not hitting max capacity */
    if(h->primeindex == (prime_table_length - 1)) {
        return 0;
    }

    newsize = primes[++(h->primeindex)];
    if(wstk_mem_zalloc((void *)&newtable, (newsize * sizeof(struct wstk_hashtable_entry *)), NULL) != WSTK_STATUS_SUCCESS) {
        log_warn("wstk_mem_zalloc()");
    }

    if(newtable != NULL) {
        /* This algorithm is not 'stable'. ie. it reverses the list when it transfers entries between the tables */
        for(i = 0; i < h->tablelength; i++) {
            while(NULL != (e = h->table[i])) {
                h->table[i] = e->next;
                index = indexFor(newsize, e->h);
                e->next = newtable[index];
                newtable[index] = e;
            }
        }
        wstk_mem_deref(h->table);
        h->table = newtable;
    } else {
        /* Plan B: realloc instead */
        newtable = h->table;
        if(wstk_mem_realloc((void *)&newtable, (newsize * sizeof(struct wstk_hashtable_entry *))) != WSTK_STATUS_SUCCESS) {
            log_warn("wstk_mem_realloc()");
            (h->primeindex)--;
            return 0;
        }

        h->table = newtable;
        memset(&newtable[h->tablelength], 0x0, ((newsize - h->tablelength) * sizeof(struct wstk_hashtable_entry *)));

        for(i = 0; i < h->tablelength; i++) {
            for(pE = &(newtable[i]), e = *pE; e != NULL; e = *pE) {
                index = indexFor(newsize, e->h);
                if(index == i) {
                    pE = &(e->next);
                } else {
                    *pE = e->next;
                    e->next = newtable[index];
                    newtable[index] = e;
                }
            }
        }
    }

    h->tablelength = newsize;
    h->loadlimit = (unsigned int) ceil(newsize * max_load_factor);

    return -1;
}

/**
 **
 **/
unsigned int wstk_hashtable_count(wstk_hashtable_t *h) {
    return h->entrycount;
}

/**
 **
 **/
static void * _wstk_hashtable_remove(wstk_hashtable_t *h, void *k, unsigned int hashvalue, unsigned int index) {
    /* TODO: consider compacting the table when the load factor drops enough, or provide a 'compact' method. */

    struct wstk_hashtable_entry *e=NULL;
    struct wstk_hashtable_entry **pE=NULL;
    void *v;

    pE = &(h->table[index]);
    e = *pE;
    while (NULL != e) {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(k, e->k))) {
            *pE = e->next;
            h->entrycount--;
            v = e->v;
            if(e->flags & WSTK_HASHTABLE_FLAG_FREE_KEY) {
                e->k = wstk_mem_deref(e->k);
            }
            if(e->flags & WSTK_HASHTABLE_FLAG_FREE_VALUE) {
                v = wstk_mem_deref(e->v);
            } else if(e->destructor) {
                e->destructor(e->v);
                v = e->v = NULL;
            }
            e = wstk_mem_deref(e);
            return v;
        }
        pE = &(e->next);
        e = e->next;
    }
    return NULL;
}

/**
 **
 **/
int wstk_hashtable_insert_destructor(wstk_hashtable_t *h, void *k, void *v, wstk_hashtable_flag_t flags, wstk_hashtable_destructor_t destructor) {
    struct wstk_hashtable_entry *e=NULL;
    unsigned int hashvalue = hash(h, k);
    unsigned index = indexFor(h->tablelength, hashvalue);

    if (flags & WSTK_HASHTABLE_DUP_CHECK) {
        _wstk_hashtable_remove(h, k, hashvalue, index);
    }

    if (++(h->entrycount) > h->loadlimit) {
        /* Ignore the return value. If expand fails, we should
         * still try cramming just this value into the existing table
         * -- we may not have memory for a larger table, but one more
         * element may be ok. Next time we insert, we'll try expanding again.*/
        hashtable_expand(h);
        index = indexFor(h->tablelength, hashvalue);
    }

    if(wstk_mem_zalloc((void *)&e, sizeof(struct wstk_hashtable_entry), NULL) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_zalloc()");
        --(h->entrycount);
        return 0;
    }

    e->h = hashvalue;
    e->k = k;
    e->v = v;
    e->flags = flags;
    e->destructor = destructor;
    e->next = h->table[index];
    h->table[index] = e;

    return -1;
}

/**
 ** returns value associated with key
 **/
void *wstk_hashtable_search(wstk_hashtable_t *h, void *k) {
    struct wstk_hashtable_entry *e = NULL;
    unsigned int hashvalue, index;

    hashvalue = hash(h, k);
    index = indexFor(h->tablelength, hashvalue);
    e = h->table[index];

    while (NULL != e) {
        /* Check hash value to short circuit heavier comparison */
        if((hashvalue == e->h) && (h->eqfn(k, e->k))) {
            return e->v;
        }
        e = e->next;
    }

    return NULL;
}

/**
 ** returns value associated with key
 **/
void *wstk_hashtable_remove(wstk_hashtable_t *h, void *k) {
    unsigned int hashvalue = hash(h, k);
    return _wstk_hashtable_remove(h, k, hashvalue, indexFor(h->tablelength, hashvalue));
}

/**
 **
 **/
wstk_hashtable_iterator_t *wstk_hashtable_next(wstk_hashtable_iterator_t **iP) {
    wstk_hashtable_iterator_t *i = *iP;

    if(i->e) {
        if((i->e = i->e->next) != 0) {
            return i;
        } else {
            i->pos++;
        }
    }
    while(i->pos < i->h->tablelength && !i->h->table[i->pos]) {
        i->pos++;
    }
    if(i->pos >= i->h->tablelength) {
        goto end;
    }
    if((i->e = i->h->table[i->pos]) != 0) {
        return i;
    }

end:
    wstk_mem_deref(i);
    *iP = NULL;

    return NULL;
}

/**
 **
 **/
wstk_hashtable_iterator_t *wstk_hashtable_first_iter(wstk_hashtable_t *h, wstk_hashtable_iterator_t *it) {
    wstk_hashtable_iterator_t *iterator = NULL;

    if(it) {
        iterator = it;
    } else {
        if(wstk_mem_zalloc((void *)&iterator, sizeof(wstk_hashtable_iterator_t), NULL) != WSTK_STATUS_SUCCESS) {
            log_error("wstk_mem_zalloc()");
        }
    }

    ht__assert(iterator);

    iterator->pos = 0;
    iterator->e = NULL;
    iterator->h = h;

    return wstk_hashtable_next(&iterator);
}

/**
 **
 **/
void wstk_hashtable_this_val(wstk_hashtable_iterator_t *i, void *val) {
    if (i->e) {
        i->e->v = val;
    }
}

/**
 **
 **/
void wstk_hashtable_this(wstk_hashtable_iterator_t *i, const void **key, size_t *klen, void **val) {
    if (i->e) {
        if (key) {
            *key = i->e->k;
        }
        if (klen) {
            *klen = (int) strlen(i->e->k);
        }
        if (val) {
            *val = i->e->v;
        }
    } else {
        if (key) {
            *key = NULL;
        }
        if (klen) {
            *klen = 0;
        }
        if (val) {
            *val = NULL;
        }
    }
}

// ===================================================================================================================================================================
// PUB
// ===================================================================================================================================================================

static inline uint32_t wstk_hash_default_int(void *ky) {
    uint32_t x = *((uint32_t *) ky);
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}

static inline int wstk_hash_equalkeys_int(void *k1, void *k2) {
    return *(uint32_t *) k1 == *(uint32_t *) k2;
}

static inline int wstk_hash_equalkeys(void *k1, void *k2) {
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

static inline int wstk_hash_equalkeys_ci(void *k1, void *k2) {
    return strcasecmp((char *) k1, (char *) k2) ? 0 : 1;
}

static inline uint32_t wstk_hash_default(void *ky) {
    unsigned char *str = (unsigned char *) ky;
    uint32_t hash = 0;
    int c;

    while ((c = *str)) {
        str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static inline uint32_t wstk_hash_default_ci(void *ky) {
    unsigned char *str = (unsigned char *) ky;
    uint32_t hash = 0;
    int c;

    while ((c = c_tolower(*str))) {
        str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// ===================================================================================================================================================================

wstk_status_t wstk_hash_init_case(wstk_hash_t **hash, bool case_sensitive) {
    if (case_sensitive) {
        return wstk_hashtable_create(hash, 16, wstk_hash_default, wstk_hash_equalkeys);
    } else {
        return wstk_hashtable_create(hash, 16, wstk_hash_default_ci, wstk_hash_equalkeys_ci);
    }
}

wstk_status_t wstk_hash_insert_destructor(wstk_hash_t *hash, const char *key, const void *data, wstk_hashtable_destructor_t destructor) {
    int r = 0;
    r = wstk_hashtable_insert_destructor(hash, (void *)wstk_str_dup(key), (void *)data, WSTK_HASHTABLE_FLAG_FREE_KEY | WSTK_HASHTABLE_DUP_CHECK, destructor);
    return (r ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
}

wstk_status_t wstk_hash_insert_ex(wstk_hash_t *hash, const char *key, const void *data, bool destroy_val) {
    int r = 0;
    int flags = (WSTK_HASHTABLE_FLAG_FREE_KEY | WSTK_HASHTABLE_DUP_CHECK);

    if(destroy_val) { flags |= WSTK_HASHTABLE_FLAG_FREE_VALUE; }

    r = wstk_hashtable_insert_destructor(hash, (void *)wstk_str_dup(key), (void *)data, flags, NULL);
    return (r ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
}

void *wstk_hash_delete(wstk_hash_t *hash, const char *key) {
    return wstk_hashtable_remove(hash, (void *) key);
}

unsigned int wstk_hash_size(wstk_hash_t *hash) {
    return wstk_hashtable_count(hash);
}

void *wstk_hash_find(wstk_hash_t *hash, const char *key) {
    return wstk_hashtable_search(hash, (void *) key);
}

bool wstk_hash_is_empty(wstk_hash_t *hash) {
    wstk_hash_index_t *hi = wstk_hash_first(hash);
    if(hi) {
        hi = wstk_mem_deref(hi);
        return false;
    }
    return true;
}

wstk_hash_index_t *wstk_hash_first(wstk_hash_t *hash) {
    return wstk_hashtable_first_iter(hash, NULL);
}

wstk_hash_index_t *wstk_hash_first_iter(wstk_hash_t *hash, wstk_hash_index_t *hi) {
    return wstk_hashtable_first_iter(hash, hi);
}

wstk_hash_index_t *wstk_hash_next(wstk_hash_index_t **hi) {
    return wstk_hashtable_next(hi);
}

void wstk_hash_this(wstk_hash_index_t *hi, const void **key, size_t *klen, void **val) {
    wstk_hashtable_this(hi, key, klen, val);
}

void wstk_hash_this_val(wstk_hash_index_t *hi, void *val) {
    wstk_hashtable_this_val(hi, val);
}

void wstk_hash_iter_free(wstk_hash_index_t *hi) {
    wstk_mem_deref(hi);
}
// -----------------------------------------------------------------------------------------------------------------------------

wstk_status_t wstk_inthash_init(wstk_inthash_t **hash) {
    return wstk_hashtable_create(hash, 16, wstk_hash_default_int, wstk_hash_equalkeys_int);
}

wstk_status_t wstk_inthash_insert_ex(wstk_inthash_t *hash, uint32_t key, const void *data, bool destroy_val) {
    int flags = (WSTK_HASHTABLE_FLAG_FREE_KEY | WSTK_HASHTABLE_DUP_CHECK);
    uint32_t *k = NULL;
    int r = 0;

    if(destroy_val) { flags |= WSTK_HASHTABLE_FLAG_FREE_VALUE; }

    if(wstk_mem_zalloc((void *)&k, sizeof(*k), NULL) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_zalloc()");
        return WSTK_STATUS_MEM_FAIL;
    }

    *k = key;
    r = wstk_hashtable_insert_destructor(hash, k, (void *)data, flags, NULL);
    return (r ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
}

void *wstk_core_inthash_delete(wstk_inthash_t *hash, uint32_t key) {
    return wstk_hashtable_remove(hash, (void *)&key);
}

void *wstk_core_inthash_find(wstk_inthash_t *hash, uint32_t key) {
    return wstk_hashtable_search(hash, (void *)&key);
}
