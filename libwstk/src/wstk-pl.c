/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-pl.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-mbuf.h>
#include <wstk-regex.h>

const wstk_pl_t pl_null = {NULL, 0};

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool wstk_pl_isset(const wstk_pl_t *pl) {
    return pl ? pl->p && pl->l : false;
}

wstk_status_t wstk_pl_set_str(wstk_pl_t *pl, const char *str) {
    if (!pl || !str) {
        return WSTK_STATUS_FALSE;
    }

    pl->p = str;
    pl->l = strlen(str);

    return WSTK_STATUS_SUCCESS;
}


wstk_status_t wstk_pl_set_mbuf(wstk_pl_t *pl, const wstk_mbuf_t *mb) {
    if (!pl || !mb) {
        return WSTK_STATUS_FALSE;
    }

    pl->p = (char *)wstk_mbuf_buf(mb);
    pl->l = wstk_mbuf_left(mb);

    return WSTK_STATUS_SUCCESS;
}

int32_t wstk_pl_i32(const wstk_pl_t *pl) {
    int32_t v = 0;
    uint32_t mul = 1;
    const char *p;
    bool neg = false;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const char ch = *--p;

        if ('0' <= ch && ch <= '9') {
            v -= mul * (ch - '0');
            mul *= 10;
        } else if (ch == '-' && p == pl->p) {
            neg = true;
            break;
        } else if (ch == '+' && p == pl->p) {
            break;
        } else {
            return 0;
        }
    }

    if (!neg && v == INT32_MIN) {
        return INT32_MIN;
    }

    return neg ? v : -v;
}

int64_t wstk_pl_i64(const wstk_pl_t *pl) {
    int64_t v = 0;
    uint64_t mul = 1;
    const char *p;
    bool neg = false;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const char ch = *--p;

        if ('0' <= ch && ch <= '9') {
            v -= mul * (ch - '0');
            mul *= 10;
        } else if (ch == '-' && p == pl->p) {
            neg = true;
            break;
        } else if (ch == '+' && p == pl->p) {
            break;
        } else {
            return 0;
        }
    }

    if (!neg && v == INT64_MIN) {
        return INT64_MIN;
    }

    return neg ? v : -v;
}


uint32_t wstk_pl_u32(const wstk_pl_t *pl) {
    uint32_t v=0, mul=1;
    const char *p;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const uint8_t c = *--p - '0';
        if (c > 9) { return 0; }
        v += mul * c;
        mul *= 10;
    }

    return v;
}


uint32_t wstk_pl_x32(const wstk_pl_t *pl) {
    uint32_t v=0, mul=1;
    const char *p;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const char ch = *--p;
        uint8_t c;

        if ('0' <= ch && ch <= '9') {
            c = ch - '0';
        } else if ('A' <= ch && ch <= 'F') {
            c = ch - 'A' + 10;
        } else if ('a' <= ch && ch <= 'f') {
            c = ch - 'a' + 10;
        } else {
            return 0;
        }

        v += mul * c;
        mul *= 16;
    }

    return v;
}

uint64_t wstk_pl_u64(const wstk_pl_t *pl) {
    uint64_t v=0, mul=1;
    const char *p;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const uint8_t c = *--p - '0';
        if (c > 9) { return 0; }
        v += mul * c;
        mul *= 10;
    }

    return v;
}

uint64_t wstk_pl_x64(const wstk_pl_t *pl) {
    uint64_t v=0, mul=1;
    const char *p;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const char ch = *--p;
        uint8_t c;

        if ('0' <= ch && ch <= '9') {
            c = ch - '0';
        } else if ('A' <= ch && ch <= 'F') {
            c = ch - 'A' + 10;
        } else if ('a' <= ch && ch <= 'f') {
            c = ch - 'a' + 10;
        } else {
            return 0;
        }

        v += mul * c;
        mul *= 16;
    }

    return v;
}

double wstk_pl_float(const wstk_pl_t *pl) {
    double v=0, mul=1;
    const char *p;
    bool neg = false;

    if (!pl || !pl->p) {
        return 0;
    }

    p = &pl->p[pl->l];
    while (p > pl->p) {
        const char ch = *--p;

        if ('0' <= ch && ch <= '9') {
            v += mul * (ch - '0');
            mul *= 10;
        } else if (ch == '.') {
            v /= mul;
            mul = 1;
        } else if (ch == '-' && p == pl->p) {
            neg = true;
        } else {
            return 0;
        }
    }

    return neg ? -v : v;
}

wstk_status_t wstk_pl_bool(bool *val, const wstk_pl_t *pl) {
    const char *tval[] = {"1", "true",  "enable",  "yes", "on"};
    const char *fval[] = {"0", "false", "disable", "no",  "off"};
    wstk_status_t status = WSTK_STATUS_FALSE;
    size_t i;

    if (!pl || !pl->p) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for (i = 0; i < ARRAY_SIZE(tval); ++i) {
        if (wstk_pl_strcasecmp(pl, tval[i]) == 0) {
            *val = true;
            status = WSTK_STATUS_SUCCESS;
        }
    }

    for (i = 0; i < ARRAY_SIZE(fval); ++i) {
        if (wstk_pl_strcasecmp(pl, fval[i]) == 0) {
            *val = false;
            status = WSTK_STATUS_SUCCESS;
        }
    }

    return status;
}

wstk_status_t wstk_pl_hex(const wstk_pl_t *pl, uint8_t *hex, size_t len) {
    size_t i = 0;

    if (!wstk_pl_isset(pl) || !hex || (pl->l != (2 * len))) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for(i = 0; i < pl->l; i += 2) {
        hex[i/2]  = wstk_ch_hex(*(pl->p + i)) << 4;
        hex[i/2] += wstk_ch_hex(*(pl->p + i +1));
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pl_strcpy(const wstk_pl_t *pl, char *str, size_t size) {
    size_t len;

    if(!pl || !pl->p || !str || !size) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    len = MIN(pl->l, size-1);

    memcpy(str, pl->p, len);
    str[len] = '\0';

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pl_strdup(char **dst, const wstk_pl_t *src) {
    wstk_status_t  status = WSTK_STATUS_SUCCESS;
    char *p = NULL;

    if(!dst || !src || !src->p) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_alloc((void *)&p, src->l + 1, NULL);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    memcpy(p, src->p, src->l);
    p[src->l] = '\0';

    *dst = p;
out:
    return status;
}

wstk_status_t wstk_pl_dup(wstk_pl_t *dst, const wstk_pl_t *src) {
    wstk_status_t  status = WSTK_STATUS_SUCCESS;
    char *p = NULL;

    if(!dst || !src || !src->p) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_alloc((void *)&p, src->l, NULL);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    memcpy(p, src->p, src->l);
    dst->p = p;
    dst->l = src->l;
out:
    return status;
}

int wstk_pl_strcmp(const wstk_pl_t *pl, const char *str) {
    wstk_pl_t s = { 0 };

    if (!pl || !str) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wstk_pl_set_str(&s, str);
    return wstk_pl_cmp(pl, &s);
}

int wstk_pl_strcasecmp(const wstk_pl_t *pl, const char *str) {
    wstk_pl_t s = { 0 };

    if (!pl || !str) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wstk_pl_set_str(&s, str);
    return wstk_pl_casecmp(pl, &s);
}

int wstk_pl_cmp(const wstk_pl_t *pl1, const wstk_pl_t *pl2) {
    if (!pl1 || !pl2) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    /* Different length -> no match */
    if (pl1->l != pl2->l) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    /* Zero-length strings are always identical */
    if (pl1->l == 0) {
        return 0;
    }

    /*
    * ~35% speed increase for fmt/pl test
    */

    /* The two pl's are the same */
    if (pl1 == pl2) {
        return 0;
    }

    /* Two different pl's pointing to same string */
    if (pl1->p == pl2->p) {
        return 0;
    }

    return 0 == memcmp(pl1->p, pl2->p, pl1->l) ? 0 : WSTK_STATUS_FALSE;
}

#ifndef HAVE_STRINGS_H
#define LOWER(d) ((d) | 0x20202020)
static int casecmp(const wstk_pl_t *pl, const char *str) {
        size_t i = 0;

        const uint32_t *p1 = (uint32_t *)pl->p;
        const uint32_t *p2 = (uint32_t *)str;
        const size_t len = pl->l & ~0x3;

        /* Skip any unaligned pointers */
        if (((size_t)pl->p) & (sizeof(void *) - 1)) {
            goto next;
        }
        if (((size_t)str) & (sizeof(void *) - 1)) {
            goto next;
        }

        /* Compare word-wise */
        for (; i<len; i+=4) {
            if (LOWER(*p1++) != LOWER(*p2++)) {
                return WSTK_STATUS_FALSE;
            }
        }

next:
        /* Compare byte-wise */
        for (; i<pl->l; i++) {
            if (tolower(pl->p[i]) != tolower(str[i])) {
                return WSTK_STATUS_FALSE;
            }
        }

        return 0;
}
#endif

int wstk_pl_casecmp(const wstk_pl_t *pl1, const wstk_pl_t *pl2) {
    if (!pl1 || !pl2) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    /* Different length -> no match */
    if (pl1->l != pl2->l) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    /* Zero-length strings are always identical */
    if (pl1->l == 0) {
        return 0;
    }

    /*
     * ~35% speed increase for fmt/pl test
     */

    /* The two pl's are the same */
    if (pl1 == pl2) {
        return 0;
    }

    /* Two different pl's pointing to same string */
    if (pl1->p == pl2->p) {
        return 0;
    }

#ifdef HAVE_STRINGS_H
        return 0 == strncasecmp(pl1->p, pl2->p, pl1->l) ? 0 : WSTK_STATUS_FALSE;
#else
        return casecmp(pl1, pl2->p);
#endif
}

const char *wstk_pl_strchr(const wstk_pl_t *pl, char c) {
    const char *p, *end;

    if (!pl) {
        return NULL;
    }

    end = pl->p + pl->l;
    for (p = pl->p; p < end; p++) {
        if (*p == c) {
            return p;
        }
    }

    return NULL;
}

const char *wstk_pl_strrchr(const wstk_pl_t *pl, char c) {
    const char *p, *end;

    if (!wstk_pl_isset(pl)) {
        return NULL;
    }

    end = pl->p + pl->l - 1;
    for (p = end; p >= pl->p; p--) {
        if (*p == c) {
            return p;
        }
    }

    return NULL;
}

const char *wstk_pl_strstr(const wstk_pl_t *pl, const char *str) {
    size_t len = wstk_str_len(str);
    size_t i = 0;

    /*case pl not set & pl is not long enough*/
    if (!wstk_pl_isset(pl) || pl->l < len) {
        return NULL;
    }

    /*case str is empty or just '\0'*/
    if (!len) {
        return pl->p;
    }

    for (i = 0; i < pl->l; ++i) {
        /*case rest of pl is not long enough*/
        if (pl->l - i < len) {
            return NULL;
        }

        if (!memcmp(pl->p + i, str, len)) {
            return pl->p + i;
        }
    }

    return NULL;
}

wstk_status_t wstk_pl_ltrim(wstk_pl_t *pl) {
    if (!wstk_pl_isset(pl)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    while (!wstk_regex(pl->p, 1, "[ \t\r\n]")) {
        ++pl->p;
        --pl->l;

        if (!pl->l) {
            return WSTK_STATUS_FALSE;
        }
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pl_rtrim(wstk_pl_t *pl) {
    if (!wstk_pl_isset(pl)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    while (!wstk_regex(pl->p + pl->l - 1, 1, "[ \t\r\n]")) {
        --pl->l;

        if (!pl->l) {
            return WSTK_STATUS_FALSE;
        }
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pl_trim(wstk_pl_t *pl) {
    wstk_status_t status;

    status  = wstk_pl_ltrim(pl);
    status |= wstk_pl_rtrim(pl);

    return status;
}

