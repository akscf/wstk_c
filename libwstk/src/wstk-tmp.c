/**
 **
 ** (C)2019 aks
 **/
#include <wstk-tmp.h>
#include <wstk-log.h>
#include <wstk-rand.h>
#include <wstk-str.h>

static unsigned char padchar[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Generate tmp name by the pattern
 *
 * @param buf       - buffer
 * @param buf_size  - buffer size
 * @param pattern   - name patter (XXXX - as a replacement marker)
 *
 * @return success or error
 **/
wstk_status_t wstk_tmp_gen_name_buf(char *buf, size_t buf_size, const char *pattern) {
    uint32_t plen = 0, i = 0;

    if(!buf || !buf_size || wstk_str_is_empty(pattern)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    plen = strlen(pattern);
    if(plen > buf_size) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    memcpy(buf, pattern, plen);
    buf[plen] = '\0';

    for(i=0; i < plen; i++) {
        if(buf[i] == 'X') {
            int32_t n = (wstk_rand_u32() % (sizeof(padchar) - 1));
            buf[i] = padchar[n];
        }
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Generate tmp name by the pattern
 *
 * @param buf       - buffer
 * @param buf_size  - buffer size
 * @param pattern   - name patter (XXXX - as a replacement marker)
 *
 * @return success or error
 **/
wstk_status_t wstk_tmp_gen_name_str(char **name, const char *pattern) {
    char *t = NULL;
    uint32_t i = 0;

    if(!name || wstk_str_is_empty(pattern)) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    i = wstk_str_len(pattern);

    t = wstk_str_ndup(pattern, i);
    if(!t) {
        return WSTK_STATUS_MEM_FAIL;
    }

    while(i >= 0) {
        if(t[i] == 'X') {
            int32_t n = (wstk_rand_u32() % (sizeof(padchar) - 1));
            t[i] = padchar[n];
        }
        i--;
    }

    *name = t;
    return WSTK_STATUS_SUCCESS;
}
