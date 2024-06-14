/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_RAND_H
#define WSTK_RAND_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t wstk_rand_u16(void);
uint32_t wstk_rand_u32(void);
uint64_t wstk_rand_u64(void);

uint32_t wstk_rand_range(uint32_t min, uint32_t max);

char wstk_rand_char(void);
void wstk_rand_str(char *str, size_t size);
void wstk_rand_bytes(uint8_t *p, size_t size);



#ifdef __cplusplus
}
#endif
#endif
