/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_SSL_H
#define WSTK_SSL_H
#include <wstk-core.h>

#ifdef WSTK_OS_WIN
#undef X509_NAME
#endif

#ifdef WSTK_USE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    uint32_t    id;
#ifdef WSTK_USE_SSL
    SSL_CTX     *ctx;
#endif
} wstk_ssl_ctx_t;

bool wstk_ssl_is_supported();




#ifdef __cplusplus
}
#endif
#endif
