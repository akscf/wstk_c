/**
 ** based on libre
 **
 ** (C)2024 aks
 **/
#include <wstk-mbuf.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-pl.h>

enum { DEFAULT_SIZE = 512 };

static void destructor__wstk_mbuf_t(void *data) {
    wstk_mbuf_t *mb = data;

    if(!mb) { return; }

    mb->buf = wstk_mem_deref(mb->buf);
}

static void mbuf_init(wstk_mbuf_t *mb) {
    if(!mb) {
        return;
    }

    mb->buf  = NULL;
    mb->size = 0;
    mb->pos  = 0;
    mb->end  = 0;
}

static int vprintf_handler(const char *p, size_t size, void *arg) {
    wstk_mbuf_t *mb = arg;
    return wstk_mbuf_write_mem(mb, (const uint8_t *)p, size);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_mbuf_create(wstk_mbuf_t **mbuf, size_t size) {
    return wstk_mbuf_alloc(mbuf, size);
}

wstk_status_t wstk_mbuf_alloc(wstk_mbuf_t **mbuf, size_t size) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *mb = NULL;

    status = wstk_mem_zalloc((void *)&mb, sizeof(wstk_mbuf_t), destructor__wstk_mbuf_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_mbuf_resize(mb, size ? size : DEFAULT_SIZE);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

#ifdef WSTK_MEM_DEBUG
    WSTK_DBG_PRINT("alloc: mbuf=%p [size=%u]\n", mb, (uint32_t)mb->size);
#endif

    *mbuf = mb;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(mb);
    }
    return status;
}

wstk_status_t wstk_mbuf_dup(wstk_mbuf_t **dst, wstk_mbuf_t *mbr) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *mb = NULL;

    if (!mbr) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_mbuf_alloc(&mb, mbr->size)) != WSTK_STATUS_SUCCESS) {
        return status;
    }

    mb->size = mbr->size;
    mb->pos  = mbr->pos;
    mb->end  = mbr->end;

    memcpy(mb->buf, mbr->buf, mbr->size);
    *dst = mb;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(mb);
    }
    return status;
}

wstk_status_t wstk_mbuf_cpy(wstk_mbuf_t *dst, wstk_mbuf_t *src) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!src || !dst) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(dst->size < src->end) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    memcpy(dst->buf, src->buf, src->end);
    dst->pos = src->end;
    dst->end = src->end;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mbuf_reset(wstk_mbuf_t *mb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    mb->buf = wstk_mem_deref(mb->buf);
    mbuf_init(mb);

    return status;
}

wstk_status_t wstk_mbuf_resize(wstk_mbuf_t *mb, size_t size) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(mb->buf) {
        status = wstk_mem_realloc((void *)&mb->buf, size);
        mb->size = size;
    } else {
        status = wstk_mem_alloc((void *)&mb->buf, size, NULL);
        mb->size = size;
    }

    return status;
}

wstk_status_t wstk_mbuf_trim(wstk_mbuf_t *mb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!mb || !mb->end || mb->end == mb->size) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    return wstk_mbuf_resize(mb, mb->end);
}

wstk_status_t wstk_mbuf_shift(wstk_mbuf_t *mb, ssize_t shift) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    uint8_t *p = NULL;
    size_t rsize;

    if(!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(((ssize_t)mb->pos + shift) < 0 || ((ssize_t)mb->end + shift) < 0) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    rsize = mb->end + shift;
    if(rsize > mb->size) {
        status = wstk_mbuf_resize(mb, rsize);
        if(status != WSTK_STATUS_SUCCESS) {
            return status;
        }
    }

    p = wstk_mbuf_buf(mb);
    memmove(p + shift, p, wstk_mbuf_left(mb));

    mb->pos += shift;
    mb->end += shift;

    return status;
}

wstk_status_t wstk_mbuf_write_mem(wstk_mbuf_t *mb, const uint8_t *buf, size_t size) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    size_t rsize;

    if(!mb || !buf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    rsize = mb->pos + size;
    if (rsize > mb->size) {
        const size_t dsize = mb->size ? (mb->size * 2) : DEFAULT_SIZE;

        status = wstk_mbuf_resize(mb, MAX(rsize, dsize));
        if(status != WSTK_STATUS_SUCCESS) {
            return status;
        }
    }

    memcpy(mb->buf + mb->pos, buf, size);

    mb->pos += size;
    mb->end  = MAX(mb->end, mb->pos);

    return status;
}

wstk_status_t wstk_mbuf_write_ptr(wstk_mbuf_t *mb, wstk_intptr_t v) {
    return wstk_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

wstk_status_t wstk_mbuf_write_u8(wstk_mbuf_t *mb, uint8_t v) {
    return wstk_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

wstk_status_t wstk_mbuf_write_u16(wstk_mbuf_t *mb, uint16_t v){
    return wstk_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

wstk_status_t wstk_mbuf_write_u32(wstk_mbuf_t *mb, uint32_t v) {
    return wstk_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

wstk_status_t wstk_mbuf_write_u64(wstk_mbuf_t *mb, uint64_t v) {
    return wstk_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

wstk_status_t wstk_mbuf_write_str(wstk_mbuf_t *mb, const char *str) {
    if(!str) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    return wstk_mbuf_write_mem(mb, (const uint8_t *)str, strlen(str));
}

wstk_status_t wstk_mbuf_write_pl(wstk_mbuf_t *mb, const wstk_pl_t *pl) {
    if(!pl) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    return wstk_mbuf_write_mem(mb, (const uint8_t *)pl->p, pl->l);
}

wstk_status_t wstk_mbuf_read_mem(wstk_mbuf_t *mb, uint8_t *buf, size_t size) {
    if(!mb || !buf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(size > wstk_mbuf_left(mb)) {
        log_error("tried to read beyond mbuf end (%u > %u)", (uint32_t)size, (uint32_t)wstk_mbuf_left(mb));
        return WSTK_STATUS_OUTOFRANGE;
    }

    memcpy(buf, mb->buf + mb->pos, size);
    mb->pos += size;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mbuf_read_ptr(wstk_mbuf_t *mb, wstk_intptr_t *val) {
    return wstk_mbuf_read_mem(mb, (uint8_t *)val, sizeof(wstk_intptr_t));
}

wstk_status_t wstk_mbuf_read_u8(wstk_mbuf_t *mb, uint8_t *val) {
    return wstk_mbuf_read_mem(mb, (uint8_t *)val, sizeof(uint8_t));
}

wstk_status_t wstk_mbuf_read_u16(wstk_mbuf_t *mb, uint16_t *val) {
    return wstk_mbuf_read_mem(mb, (uint8_t *)val, sizeof(uint16_t));
}

wstk_status_t wstk_mbuf_read_u32(wstk_mbuf_t *mb, uint32_t *val) {
    return wstk_mbuf_read_mem(mb, (uint8_t *)val, sizeof(uint32_t));
}

wstk_status_t wstk_mbuf_read_u64(wstk_mbuf_t *mb, uint64_t *val) {
    return wstk_mbuf_read_mem(mb, (uint8_t *)val, sizeof(uint64_t));
}

wstk_status_t wstk_mbuf_read_str(wstk_mbuf_t *mb, char *str, size_t size) {
    if(!mb || !str) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    while (size--) {
        uint8_t c;

        wstk_mbuf_read_u8(mb, &c);
        *str++ = c;

        if ('\0' == c) { break; }
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mbuf_strdup(wstk_mbuf_t *mb, char **strp, size_t len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *str = NULL;

    if(!mb || !strp) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_alloc((void *)&str, len + 1, NULL);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = wstk_mbuf_read_mem(mb, (uint8_t *)str, len);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    str[len] = '\0';
    *strp = str;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(str);
    }
    return status;
}


wstk_status_t wstk_mbuf_write_pl_skip(wstk_mbuf_t *mb, const wstk_pl_t *pl, const wstk_pl_t *skip) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_pl_t r = { 0 };

    if(!pl || !skip) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if (pl->p > skip->p || (skip->p + skip->l) > (pl->p + pl->l)) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    r.p = pl->p;
    r.l = skip->p - pl->p;

    status = wstk_mbuf_write_mem(mb, (const uint8_t *)r.p, r.l);
    if (status != WSTK_STATUS_SUCCESS) {
        return status;
    }

    r.p = skip->p + skip->l;
    r.l = pl->p + pl->l - r.p;

    return wstk_mbuf_write_mem(mb, (const uint8_t *)r.p, r.l);
}

wstk_status_t wstk_mbuf_fill(wstk_mbuf_t *mb, uint8_t c, size_t n) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    size_t rsize;

    if (!mb || !n) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    rsize = mb->pos + n;

    if(rsize > mb->size) {
        const size_t dsize = mb->size ? (mb->size * 2) : DEFAULT_SIZE;

        status = wstk_mbuf_resize(mb, MAX(rsize, dsize));
        if (status != WSTK_STATUS_SUCCESS) {
            return status;
        }
    }

    memset(mb->buf + mb->pos, c, n);

    mb->pos += n;
    mb->end  = MAX(mb->end, mb->pos);

    return status;
}

wstk_status_t wstk_mbuf_set_posend(wstk_mbuf_t *mb, size_t pos, size_t end) {
    if (!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if (pos > end) {
        log_warn("set_posend: pos %u > end %u\n", (uint32_t)pos, (uint32_t)end);
        return WSTK_STATUS_OUTOFRANGE;
    }

    if (end > mb->size) {
        log_warn("set_posend: end %u > size %u\n", (uint32_t)end, (uint32_t)mb->size);
        return WSTK_STATUS_OUTOFRANGE;
    }

    mb->pos = pos;
    mb->end = end;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_mbuf_vprintf(wstk_mbuf_t *mb, const char *fmt, va_list ap) {
    if (!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    return wstk_vhprintf(fmt, ap, vprintf_handler, mb);
}

wstk_status_t wstk_mbuf_printf(wstk_mbuf_t *mb, const char *fmt, ...) {
    wstk_status_t status;
    va_list ap;

    if (!mb) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    va_start(ap, fmt);
    status = wstk_vhprintf(fmt, ap, vprintf_handler, mb);
    va_end(ap);

    return status;
}
