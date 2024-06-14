/**
 **
 ** (C)2024 aks
 **/
#include <wstk-escape.h>
#include <wstk-log.h>
#include <wstk-mbuf.h>
#include <wstk-mem.h>
#include <wstk-str.h>

/* Portable character check (remember EBCDIC). Do not use isalnum() because
   its behavior is altered by the current locale.
   See http://tools.ietf.org/html/rfc3986#section-2.3
*/
static bool curl_isunreserved(uint8_t in) {
    switch (in) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '-': case '.': case '_': case '~':
            return true;
        default:
            break;
  }
  return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Escape string
 *
 * @param out       - a new string
 * @param str       - the string
 * @param str_len   - str len
 *
 * @return new string or NULL
 **/
wstk_status_t wstk_escape(char **out, const char *str, size_t str_len) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *tbuf = NULL;
    uint32_t i=0;

    if(!str || !str_len || !out) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((st = wstk_mbuf_alloc(&tbuf, str_len * 2)) != WSTK_STATUS_SUCCESS) {
        return st;
    }

    for(i=0; i < str_len; i++) {
        uint8_t c = str[i];
        if(curl_isunreserved(c)) {
            wstk_mbuf_write_u8(tbuf, c);
        } else {
            wstk_mbuf_printf(tbuf, "%%%02X", c & 0xff);
        }
    }

    tbuf->pos = 0;
    st = wstk_mbuf_strdup(tbuf, out, tbuf->end);
    wstk_mem_deref(tbuf);

    return st;
}

/**
 * Unescape string
 *
 * @param out       - a new string
 * @param str       - the string
 * @param str_len   - str len
 *
 * @return new string or NULL
 **/
wstk_status_t wstk_unescape(char **out, const char *str, size_t str_len) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    wstk_mbuf_t *tbuf = NULL;
    uint32_t  i = 0;

    if(!str || !str_len || !out) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((st = wstk_mbuf_alloc(&tbuf, str_len)) != WSTK_STATUS_SUCCESS) {
        return st;
    }

    for(i=0; i < str_len; i++) {
        if(str[i] == '%') {
            if(i + 2 < str_len) {
                const uint8_t hi = wstk_ch_hex(str[++i]);
                const uint8_t lo = wstk_ch_hex(str[++i]);
                const uint8_t ch = (hi<<4 | lo);
                wstk_mbuf_write_u8(tbuf, ch);
            } else {
                log_error("malformed escape sequence");
                st = WSTK_STATUS_FALSE;
                break;
            }
        } else {
            wstk_mbuf_write_u8(tbuf, (uint8_t )str[i]);
        }
    }

    if(st == WSTK_STATUS_SUCCESS) {
        tbuf->pos = 0;
        st = wstk_mbuf_strdup(tbuf, out, tbuf->end);
        wstk_mem_deref(tbuf);
    }

    return st;
}
