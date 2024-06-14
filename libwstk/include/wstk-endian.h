/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_ENDIAN_H
#define WSTK_ENDIAN_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t wstk_htols(uint16_t v);
uint32_t wstk_htoll(uint32_t v);

uint16_t wstk_ltohs(uint16_t v);
uint32_t wstk_ltohl(uint32_t v);

uint64_t wstk_htonll(uint64_t v);
uint64_t wstk_ntohll(uint64_t v);



#ifdef __cplusplus
}
#endif
#endif
