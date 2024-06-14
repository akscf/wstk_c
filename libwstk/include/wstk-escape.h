/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_ESCAPE_H
#define WSTK_ESCAPE_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

wstk_status_t wstk_escape(char **out, const char *str, size_t str_len);
wstk_status_t wstk_unescape(char **out, const char *str, size_t str_len);



#ifdef __cplusplus
}
#endif
#endif
