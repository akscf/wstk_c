/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_SLEEP_H
#define WSTK_SLEEP_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Suspends current thread
 *
 * @param msec - delay in milliseconds
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_msleep(uint32_t msec);



#ifdef __cplusplus
}
#endif
#endif
