/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_MBUF_H
#define WSTK_MBUF_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t *wstk_mbuf_buf(const wstk_mbuf_t *mb) {
     return mb ? mb->buf + mb->pos : (uint8_t *)NULL;
}
static inline size_t wstk_mbuf_left(const wstk_mbuf_t *mb) {
    return (mb && (mb->end > mb->pos)) ? (mb->end - mb->pos) : 0;
}
static inline size_t wstk_mbuf_space(const wstk_mbuf_t *mb) {
    return (mb && (mb->size > mb->pos)) ? (mb->size - mb->pos) : 0;
}
static inline size_t wstk_mbuf_pos(const wstk_mbuf_t *mb) {
    return mb ? mb->pos : 0;
}
static inline size_t wstk_mbuf_end(const wstk_mbuf_t *mb) {
    return mb ? mb->end : 0;
}
static inline size_t wstk_mbuf_size(const wstk_mbuf_t *mb) {
    return mb ? mb->size : 0;
}
static inline void wstk_mbuf_clean(wstk_mbuf_t *mb) {
    if(!mb) { return; }
    mb->pos = 0;
    mb->end = 0;
}

static inline void wstk_mbuf_set_pos(wstk_mbuf_t *mb, size_t pos)  { mb->pos = pos; }
static inline void wstk_mbuf_set_end(wstk_mbuf_t *mb, size_t end)  { mb->end = end;}
static inline void wstk_mbuf_advance(wstk_mbuf_t *mb, ssize_t n)   { mb->pos += n; }
static inline void wstk_mbuf_rewind(wstk_mbuf_t *mb)               { mb->pos = mb->end = 0; }
static inline void wstk_mbuf_skip_to_end(wstk_mbuf_t *mb)          { mb->pos = mb->end; }

wstk_status_t wstk_mbuf_create(wstk_mbuf_t **mbuf, size_t size);

wstk_status_t wstk_mbuf_alloc(wstk_mbuf_t **mbuf, size_t size);
wstk_status_t wstk_mbuf_dup(wstk_mbuf_t **dst, wstk_mbuf_t *mbr);
wstk_status_t wstk_mbuf_cpy(wstk_mbuf_t *dst, wstk_mbuf_t *src);
wstk_status_t wstk_mbuf_resize(wstk_mbuf_t *mb, size_t size);
wstk_status_t wstk_mbuf_shift(wstk_mbuf_t *mb, ssize_t shift);

wstk_status_t wstk_mbuf_write_mem(wstk_mbuf_t *mb, const uint8_t *buf, size_t size);
wstk_status_t wstk_mbuf_write_ptr(wstk_mbuf_t *mb, wstk_intptr_t v);
wstk_status_t wstk_mbuf_write_u8(wstk_mbuf_t *mb, uint8_t v);
wstk_status_t wstk_mbuf_write_u16(wstk_mbuf_t *mb, uint16_t v);
wstk_status_t wstk_mbuf_write_u32(wstk_mbuf_t *mb, uint32_t v);
wstk_status_t wstk_mbuf_write_u64(wstk_mbuf_t *mb, uint64_t v);
wstk_status_t wstk_mbuf_write_str(wstk_mbuf_t *mb, const char *str);
wstk_status_t wstk_mbuf_write_pl(wstk_mbuf_t *mb, const wstk_pl_t *pl);

wstk_status_t wstk_mbuf_read_mem(wstk_mbuf_t *mb, uint8_t *buf, size_t size);
wstk_status_t wstk_mbuf_read_ptr(wstk_mbuf_t *mb, wstk_intptr_t *val);
wstk_status_t wstk_mbuf_read_u8(wstk_mbuf_t *mb,  uint8_t  *val);
wstk_status_t wstk_mbuf_read_u16(wstk_mbuf_t *mb, uint16_t *val);
wstk_status_t wstk_mbuf_read_u32(wstk_mbuf_t *mb, uint32_t *val);
wstk_status_t wstk_mbuf_read_u64(wstk_mbuf_t *mb, uint64_t *val);
wstk_status_t wstk_mbuf_read_str(wstk_mbuf_t *mb, char *str, size_t size);
wstk_status_t wstk_mbuf_strdup(wstk_mbuf_t *mb, char **strp, size_t len);

wstk_status_t wstk_mbuf_write_pl_skip(wstk_mbuf_t *mb, const wstk_pl_t *pl, const wstk_pl_t *skip);
wstk_status_t wstk_mbuf_fill(wstk_mbuf_t *mb, uint8_t c, size_t n);
wstk_status_t wstk_mbuf_set_posend(wstk_mbuf_t *mb, size_t pos, size_t end);

wstk_status_t wstk_mbuf_vprintf(wstk_mbuf_t *mb, const char *fmt, va_list ap);
wstk_status_t wstk_mbuf_printf(wstk_mbuf_t *mb, const char *fmt, ...);

wstk_status_t mbuf_trim(wstk_mbuf_t *mb);
wstk_status_t wstk_mbuf_reset(wstk_mbuf_t *mb);



#ifdef __cplusplus
}
#endif
#endif
