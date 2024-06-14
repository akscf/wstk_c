/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_REGEX_H
#define WSTK_REGEX_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

wstk_status_t wstk_regex(const char *ptr, size_t len, const char *expr, ...);

#ifdef __cplusplus
}
#endif
#endif
