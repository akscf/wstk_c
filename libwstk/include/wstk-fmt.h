/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_FMT_H
#define WSTK_FMT_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int(wstk_vprintf_h)(const char *p, size_t size, void *arg);

typedef struct wstk_printf_s {
    wstk_vprintf_h  *vph;
    void            *arg;
} wstk_printf_t;

typedef int(wstk_printf_h)(wstk_printf_t *pf, void *arg);

int wstk_vhprintf(const char *fmt, va_list ap, wstk_vprintf_h *vph, void *arg);
int wstk_vfprintf(FILE *stream, const char *fmt, va_list ap);
int wstk_vprintf(const char *fmt, va_list ap);
int wstk_vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

int wstk_fprintf(FILE *stream, const char *fmt, ...);
int wstk_printf(const char *fmt, ...);
int wstk_snprintf(char *str, size_t size, const char *fmt, ...);

wstk_status_t wstk_vsdprintf(char **strp, const char *fmt, va_list ap);
wstk_status_t wstk_hprintf(wstk_printf_t *pf, const char *fmt, ...);
wstk_status_t wstk_sdprintf(char **strp, const char *fmt, ...);



#ifdef __cplusplus
}
#endif
#endif
