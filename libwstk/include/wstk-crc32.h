/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_MD5_H
#define WSTK_MD5_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t wstk_crc32_update(uint32_t crc, const void *input, uint32_t size);



#ifdef __cplusplus
}
#endif
#endif
