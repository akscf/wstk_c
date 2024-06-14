/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-base64.h>
#include <wstk-log.h>
#include <wstk-fmt.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-str.h>

static const char b64_table[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";


/* convert char -> 6-bit value */
static inline uint32_t b64val(char c) {
    if ('A' <= c && c <= 'Z')
        return c - 'A' + 0;
    else if ('a' <= c && c <= 'z')
        return c - 'a' + 26;
    else if ('0' <= c && c <= '9')
        return c - '0' + 52;
    else if ('+' == c || '-' == c)
        return 62;
    else if ('/' == c || '_' == c)
        return 63;
    else if ('=' == c)
        return 1<<24; /* special trick */
    else
        return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_base64_encode(const uint8_t *in, size_t ilen, char *out, size_t *olen) {
    const uint8_t *in_end = (in + ilen);
    const char *o = out;

    if(!in || !out || !olen) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(*olen < BASE64_ENC_SIZE(ilen)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for (; in < in_end; ) {
        uint32_t v;
        int pad = 0;

        v  = *in++ << 16;

        if(in < in_end) { v |= *in++ << 8; }
        else { ++pad; }

        if(in < in_end) { v |= *in++ << 0; }
        else { ++pad; }

        *out++ = b64_table[v>>18 & 0x3f];
        *out++ = b64_table[v>>12 & 0x3f];
        *out++ = (pad >= 2) ? '=' : b64_table[v>>6 & 0x3f];
        *out++ = (pad >= 1) ? '=' : b64_table[v>>0 & 0x3f];
    }

    *olen = (out - o);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Encode string to base64
 *
 * @param in    - source data
 * @param ilen  - source data lenght
 * @param ostr  - a new string in base64
 * @param olen  - a new string lenght (or NULL)
 *
 * @return sucees ot some error
 **/
wstk_status_t wstk_base64_encode_str(const char *in, size_t ilen, char **ostr, size_t *olen) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    size_t osz = BASE64_ENC_SIZE(ilen);
    char *obuf = NULL;

    if(!in || !ilen || !ostr) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_mem_zalloc((void *)&obuf, osz + 1, NULL)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_base64_encode((uint8_t *)in, ilen, obuf, &osz)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    obuf[osz] = 0x0;

    *ostr = obuf;
    if(olen) { *olen = osz; }
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(obuf);
    }
    return status;
}


wstk_status_t wstk_base64_decode(const char *in, size_t ilen, uint8_t *out, size_t *olen) {
    const char *in_end = in + ilen;
    const uint8_t *o = out;

    if(!in || !out || !olen) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(*olen < BASE64_DEC_SIZE(ilen)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for (; in+3 < in_end; ) {
        uint32_t v;

        v  = b64val(*in++) << 18;
        v |= b64val(*in++) << 12;
        v |= b64val(*in++) << 6;
        v |= b64val(*in++) << 0;

        *out++ = v>>16;
        if(!(v & (1<<30))) {
            *out++ = (v>>8) & 0xff;
        }
        if(!(v & (1<<24))) {
            *out++ = (v>>0) & 0xff;
        }
    }

    *olen = (out - o);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Decode base64 to a string
 *
 * @param in    - data in bs64
 * @param ilen  - data lenght
 * @param ostr  - a new string
 * @param olen  - a new string lenght (or NULL)
 *
 * @return sucees ot some error
 **/
wstk_status_t wstk_base64_decode_str(const char *in, size_t ilen, char **ostr, size_t *olen) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    size_t osz = BASE64_DEC_SIZE(ilen);
    char *obuf = NULL;

    if(!in || !ilen || !ostr) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_mem_zalloc((void *)&obuf, osz + 2, NULL)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    if((status = wstk_base64_decode(in, ilen, (uint8_t *)obuf, &osz)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    obuf[osz] = 0x0;

    *ostr = obuf;
    if(olen) { *olen = osz; }
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(obuf);
    }
    return status;
}


