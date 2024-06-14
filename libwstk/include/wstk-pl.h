/**
 ** Pointer length object
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_PL_H
#define WSTK_PL_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const wstk_pl_t pl_null;

#define WSTK_PL(s) {(s), sizeof((s))-1}
#define WSTK_PL_INIT {NULL, 0}

static inline void wstk_pl_advance(wstk_pl_t *pl, ssize_t n) {
    pl->p += n;
    pl->l -= n;
}

wstk_status_t wstk_pl_set_str(wstk_pl_t *pl, const char *str);
wstk_status_t wstk_pl_set_mbuf(wstk_pl_t *pl, const wstk_mbuf_t *mb);

int32_t  wstk_pl_i32(const wstk_pl_t *pl);
int64_t  wstk_pl_i64(const wstk_pl_t *pl);
uint32_t wstk_pl_u32(const wstk_pl_t *pl);
uint32_t wstk_pl_x32(const wstk_pl_t *pl);
uint64_t wstk_pl_u64(const wstk_pl_t *pl);
uint64_t wstk_pl_x64(const wstk_pl_t *pl);
double   wstk_pl_float(const wstk_pl_t *pl);

bool wstk_pl_isset(const wstk_pl_t *pl);
wstk_status_t wstk_pl_bool(bool *val, const wstk_pl_t *pl);
wstk_status_t wstk_pl_hex(const wstk_pl_t *pl, uint8_t *hex, size_t len);
wstk_status_t wstk_pl_strcpy(const wstk_pl_t *pl, char *str, size_t size);
wstk_status_t wstk_pl_strdup(char **dst, const wstk_pl_t *src);
wstk_status_t wstk_pl_dup(wstk_pl_t *dst, const wstk_pl_t *src);

int wstk_pl_strcmp(const wstk_pl_t *pl, const char *str);
int wstk_pl_strcasecmp(const wstk_pl_t *pl, const char *str);
int wstk_pl_cmp(const wstk_pl_t *pl1, const wstk_pl_t *pl2);
int wstk_pl_casecmp(const wstk_pl_t *pl1, const wstk_pl_t *pl2);
const char *wstk_pl_strchr(const wstk_pl_t *pl, char c);
const char *wstk_pl_strrchr(const wstk_pl_t *pl, char c);
const char *wstk_pl_strstr(const wstk_pl_t *pl, const char *str);

wstk_status_t wstk_pl_ltrim(wstk_pl_t *pl);
wstk_status_t wstk_pl_rtrim(wstk_pl_t *pl);
wstk_status_t wstk_pl_rtrim(wstk_pl_t *pl);




#ifdef __cplusplus
}
#endif
#endif
