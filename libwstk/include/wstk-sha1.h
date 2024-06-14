/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_SHA1_H
#define WSTK_SHA1_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif


#define WSTK_SHA1_DIGEST_SIZE        20
#define WSTK_SHA1_DIGEST_STRING_SIZE 40

typedef struct SHA1_CTX_S {
    uint32_t state[5];      /**< Context state */
    uint32_t count[2];      /**< Counter       */
    uint8_t  buffer[64];    /**< SHA-1 buffer  */
} SHA1_CTX;
typedef struct SHA1_CTX_S wstk_sha1_t;

wstk_status_t wstk_sha1_init(wstk_sha1_t *ctx);
wstk_status_t wstk_sha1_update(wstk_sha1_t *ctx, const uint8_t *data, size_t len);
wstk_status_t wstk_sha1_final(wstk_sha1_t *ctx, uint8_t *digest);

wstk_status_t wstk_sha1_digest(uint8_t *digest, const uint8_t *data, size_t len);
wstk_status_t wstk_sha1_digest2hex(uint8_t *hex_str, uint8_t *digest);
bool wstk_sha1_is_equal(uint8_t *dig1, uint8_t *dig2);



#ifdef __cplusplus
}
#endif
#endif
