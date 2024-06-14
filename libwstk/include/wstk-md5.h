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

#define WSTK_MD5_DIGEST_SIZE        16
#define WSTK_MD5_DIGEST_STRING_SIZE 32

typedef unsigned char   md5_byte_t;
typedef unsigned int    md5_word_t;
typedef struct md5_state_s {
    md5_word_t  count[2];   /* message length in bits, lsw first */
    md5_word_t  abcd[4];    /* digest buffer */
    md5_byte_t  buf[64];    /* accumulate block */
} md5_state_t;
typedef struct md5_state_s wstk_md5_t;

wstk_status_t wstk_md5_init(wstk_md5_t *ctx);
wstk_status_t wstk_md5_update(wstk_md5_t *ctx, const uint8_t *data, size_t len);
wstk_status_t wstk_md5_final(wstk_md5_t *ctx, uint8_t *digest);

wstk_status_t wstk_md5_digest(uint8_t *digest, const uint8_t *data, size_t len);
wstk_status_t wstk_md5_digest2hex(uint8_t *hex, uint8_t *digest);
bool wstk_md5_is_equal(uint8_t *dig1, uint8_t *dig2);



#ifdef __cplusplus
}
#endif
#endif
