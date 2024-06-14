/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_CODEPAGE_H
#define WSTK_CODEPAGE_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t        id;
    const char      *name;
} wstk_codepage_entry_t;

uint32_t wstk_system_codepage_id();
const char *wstk_system_codepage_name();
uint32_t wstk_get_system_cp_now();



#ifdef __cplusplus
}
#endif
#endif
