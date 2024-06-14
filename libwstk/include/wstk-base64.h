/**
 * (C)2018 aks
 **/
#ifndef WSTK_BASE64_H
#define WSTK_BASE64_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASE64_ENC_SIZE(n) (4*((n+2)/3))
#define BASE64_DEC_SIZE(n) (3*(n/4))

wstk_status_t wstk_base64_encode(const uint8_t *in, size_t ilen, char *out, size_t *olen);
wstk_status_t wstk_base64_decode(const char *in, size_t ilen, uint8_t *out, size_t *olen);

wstk_status_t wstk_base64_encode_str(const char *in, size_t ilen, char **ostr, size_t *olen);
wstk_status_t wstk_base64_decode_str(const char *in, size_t ilen, char **ostr, size_t *olen);




#ifdef __cplusplus
}
#endif
#endif
