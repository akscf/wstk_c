/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_TMP_H
#define WSTK_TMP_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

wstk_status_t wstk_tmp_gen_name_buf(char *buf, size_t buf_size, const char *pattern);
wstk_status_t wstk_tmp_gen_name_str(char **name, const char *pattern);


#ifdef __cplusplus
}
#endif
#endif
