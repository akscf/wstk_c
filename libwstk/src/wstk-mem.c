/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-mem.h>
#include <wstk-log.h>

static const uint32_t mem_magic = 0xE7fB9AC4;
typedef struct wstk_mem_s {

#ifdef WSTK_MEM_DEBUG
    uint32_t                magic;
    size_t                  size;
#endif
    uint32_t                refs;
    wstk_mem_destructor_h   dh;
} wstk_mem_t;

enum {
#if defined(__x86_64__)
        /* Use 16-byte alignment on x86-x32 as well */
        mem_alignment = 16u,
#else
        mem_alignment = (sizeof(void*) >= 8u ? 16u : 8u),
#endif
        alignment_mask  = mem_alignment - 1u,
        mem_header_size = (sizeof(wstk_mem_t) + alignment_mask) & (~(size_t)alignment_mask)
};

#ifdef WSTK_MEM_DEBUG
 #define MAGIC_CHECK(_m)                                                \
        if (mem_magic != (_m)->magic) {                                 \
                WSTK_DBG_PRINT("%s: magic check failed 0x%08x (%p)\n",       \
                        __func__, (_m)->magic, get_mem_data((_m)));     \
                WSTK_BREAKPOINT;                                        \
        }

 #define MAGIC_SET(_m) _m->magic = mem_magic;
 #define DBG_SIZE_SET(_m,_s) _m->size = _s;
#else
 #define MAGIC_CHECK(_m)
 #define MAGIC_SET(_m)
 #define DBG_SIZE_SET(_m,_s)
#endif

static inline wstk_mem_t *get_mem(void *p) {
    return (wstk_mem_t *)(void *)(((uint8_t *)p) - mem_header_size);
}

static inline void *get_mem_data(wstk_mem_t *m) {
    return (void *)(((uint8_t *)m) + mem_header_size);
}


// -------------------------------------------------------------------------------------------------------------------
static void *mem_alloc(size_t size, wstk_mem_destructor_h dh) {
    wstk_mem_t *m = NULL;

    m = malloc(mem_header_size + size);
    if(!m) { return NULL; }

#ifdef WSTK_MEM_DEBUG
    WSTK_DBG_PRINT("alloc: mem=%p [dh=%p, size=%d, alignment_mask=%d, header_size=%d]\n", m, dh, (uint32_t)size, (int)alignment_mask, (int)mem_header_size);
#endif

    m->refs =1;
    m->dh = dh;

    MAGIC_SET(m);
    DBG_SIZE_SET(m, size);

    return get_mem_data(m);
}

static void *mem_zalloc(size_t size, wstk_mem_destructor_h dh) {
    void *p = NULL;

    p = mem_alloc(size, dh);
    if(!p) { return NULL; }

    memset(p, 0x0, size);
    return p;
}

static void *mem_realloc(void *mem, size_t size) {
    wstk_mem_t *m = NULL, *m2 = NULL;

    if(!mem) {
        if(size > 0) {
            return mem_alloc(size, NULL);
        }
        return NULL;
    }

    m = get_mem(mem);
    MAGIC_CHECK(m);

    m2 = realloc(m, mem_header_size + size);
    if(!m2) { return NULL; }

    return get_mem_data(m2);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_mem_alloc(void **mem, size_t size, wstk_mem_destructor_h dh) {
    void *m = NULL;

    m = mem_alloc(size, dh);
    if(!m) { return WSTK_STATUS_MEM_FAIL; }

    *mem = m;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mem_zalloc(void **mem, size_t size, wstk_mem_destructor_h dh) {
    void *m = NULL;

    m = mem_zalloc(size, dh);
    if(!m) { return WSTK_STATUS_MEM_FAIL; }

    *mem = m;
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mem_realloc(void **mem, size_t size) {
    void *m = NULL;

    m = mem_realloc(*mem, size);
    if(!m) { return WSTK_STATUS_MEM_FAIL; }

    *mem = m;
    return WSTK_STATUS_SUCCESS;
}

/**
 * convert conventional c-pointer to wstk_mem
 * after that it can be destroyed through wstk_mem_deref
 **/
void *wstk_mem_wrap(void *cptr, size_t size, wstk_mem_destructor_h dh) {
    void *m = NULL;

    if(!cptr || !size){
        return NULL;
    }

    m = mem_alloc(size, dh);
    if(!m) { return NULL; }

    memcpy(m, cptr, size);
    free(cptr);

    return m;
}

void *wstk_mem_ref(void *mem) {
    wstk_mem_t *m = NULL;

    if(!mem){
        return NULL;
    }

    m = get_mem(mem);
    MAGIC_CHECK(m);

#ifdef WSTK_MEM_DEBUG
    WSTK_DBG_PRINT("mem-ref: mem=%p [dh=%p, refs=%d, size=%d]\n", m, m->dh, m->refs, (uint32_t)m->size);
#endif

    ++m->refs;
    return mem;
}

void *wstk_mem_deref(void *mem) {
    wstk_mem_t *m = NULL;

    if(!mem){
        return NULL;
    }

    m = get_mem(mem);
    MAGIC_CHECK(m);

#ifdef WSTK_MEM_DEBUG
    WSTK_DBG_PRINT("mem-deref: mem=%p [dh=%p, refs=%d, size=%d]\n", m, m->dh, m->refs, (uint32_t)m->size);
#endif

    if(--m->refs > 0) {
        return NULL;
    }

    if(m->dh) {
        m->dh(mem);
    }

    free(m);
    m = NULL;

    return NULL;
}

wstk_status_t wstk_mem_bzero(void *mem, size_t size) {
    if(!mem) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    memset(mem, 0x0, size);

    return WSTK_STATUS_SUCCESS;
}
