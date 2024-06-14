/**
 **
 ** (C)2019 aks
 **/
#include <wstk-str.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>

/*
 * http://www.azillionmonkeys.com/qed/asmexample.html
 */
static int32_t l_iupper(int32_t eax) {
    int32_t ebx = (0x7f7f7f7ful & eax) + 0x05050505ul;
    ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
    ebx = ((ebx & ~eax) >> 2) & 0x20202020ul;
    return eax - ebx;
}

static int32_t l_ilower(int32_t eax) {
    int32_t ebx = (0x7f7f7f7ful & eax) + 0x25252525ul;
    ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
    ebx = ((ebx & ~eax) >> 2) & 0x20202020ul;
    return eax + ebx;
}

/* freeswitch functions */
static char unescape_char(char escaped) {
    char unescaped;
    switch (escaped) {
        case 'n': unescaped = '\n';
        break;

        case 'r': unescaped = '\r';
        break;

        case 't': unescaped = '\t';
        break;

        case 's': unescaped = ' ';
        break;

        default:
                unescaped = escaped;
    }
    return unescaped;
}

/* Helper function used when separating strings to remove quotes,
  leading trailing spaces, and to convert escaped characters. */
#define ESCAPE_META '\\'
static char *cleanup_separated_string(char *str, char delim) {
    char *ptr;
    char *dest;
    char *start;
    char *end = NULL;
    int inside_quotes = 0;

    /* Skip initial whitespace */
    for(ptr = str; *ptr == ' '; ++ptr) {
    }

    for(start = dest = ptr; *ptr; ++ptr) {
        char e;
        int esc = 0;

        if(*ptr == ESCAPE_META) {
            e = *(ptr + 1);
            if (e == '\'' || e == '"' || (delim && e == delim) || e == ESCAPE_META || (e = unescape_char(*(ptr + 1))) != *(ptr + 1)) {
                ++ptr;
                *dest++ = e;
                end = dest;
                esc++;
            }
        }

        if(!esc) {
            if (*ptr == '\'' && (inside_quotes || strchr(ptr+1, '\''))) {
                if ((inside_quotes = (1 - inside_quotes))) { end = dest; }
            } else {
                *dest++ = *ptr;
                if (*ptr != ' ' || inside_quotes) { end = dest; }
            }
        }
    }

    if(end) { *end = '\0'; }

    return start;
}

/* Separate a string using a delimiter that is a space */
static uint32_t separate_string_blank_delim(char *buf, char **array, unsigned int arraylen) {
    enum tokenizer_state {
            START, SKIP_INITIAL_SPACE, FIND_DELIM, SKIP_ENDING_SPACE
    } state = START;

    char *ptr = buf;
    int inside_quotes = 0;
    uint32_t count = 0, i = 0;

    while (*ptr && count < arraylen) {
        switch(state) {
            case START:
                array[count++] = ptr;
                state = SKIP_INITIAL_SPACE;
                break;

            case SKIP_INITIAL_SPACE:
                if (*ptr == ' ') { ++ptr; }
                else { state = FIND_DELIM; }
            break;

            case FIND_DELIM:
                if (*ptr == ESCAPE_META) { ++ptr; }
                else if (*ptr == '\'')   { inside_quotes = (1 - inside_quotes); }
                else if (*ptr == ' ' && !inside_quotes) { *ptr = '\0'; state = SKIP_ENDING_SPACE; }
                ++ptr;
            break;

            case SKIP_ENDING_SPACE:
                if (*ptr == ' ') { ++ptr; }
                else { state = START; }
            break;
        }
    }

    /* strip quotes, escaped chars and leading / trailing spaces */
    for(i = 0; i < count; ++i) {
        array[i] = cleanup_separated_string(array[i], 0);
    }

    return count;
}

/* Separate a string using a delimiter that is not a space */
static uint32_t separate_string_char_delim(char *buf, char delim, char **array, unsigned int arraylen) {
    enum tokenizer_state {
        START, FIND_DELIM
    } state = START;

    char *ptr = buf;
    int inside_quotes = 0;
    uint32_t count = 0, i = 0;

    while (*ptr && count < arraylen) {
        switch (state) {
            case START:
                array[count++] = ptr;
                state = FIND_DELIM;
            break;

            case FIND_DELIM:
                /* escaped characters are copied verbatim to the destination string */
                if (*ptr == ESCAPE_META) { ++ptr; }
                else if (*ptr == '\'' && (inside_quotes || strchr(ptr+1, '\''))) { inside_quotes = (1 - inside_quotes); }
                else if (*ptr == delim && !inside_quotes) { *ptr = '\0'; state = START; }
                ++ptr;
            break;
        }
    }

    /* strip quotes, escaped chars and leading / trailing spaces */
    for (i = 0; i < count; ++i) {
        array[i] = cleanup_separated_string(array[i], delim);
    }

    return count;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t wstk_ch_hex(char ch) {
    if ('0' <= ch && ch <= '9') {
        return ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 10;
    } else if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return 0;
}

size_t wstk_str_len(const char *s) {
    return (s == NULL ? 0 : strlen(s));
}

char *wstk_str_dup(const char *s) {
    char *res = NULL;
    size_t len = 0;

    if(wstk_str_is_empty(s)) {
        return NULL;
    }

    len = strlen(s);
    if(wstk_mem_alloc((void *)&res, len + 1, NULL) != WSTK_STATUS_SUCCESS) {
        return NULL;
    }

    memcpy(res, s, len);
    res[len] = '\0';

    return res;
}

char *wstk_str_ndup(const char *s, size_t len) {
    char *res = NULL;
    const char *end = NULL;

    if(!s || !len) {
        return NULL;
    }

    end = (char *) memchr(s, '\0', len);
    if (end != NULL) {
        len = end - s;
    }

    if(wstk_mem_alloc((void *)&res, len + 1, NULL) != WSTK_STATUS_SUCCESS) {
        return NULL;
    }

    memcpy(res, s, len);
    res[len] = '\0';

    return res;
}

/**
 * conventional c-string to wstk_mem
 * after that it can be destroyed through wstk_mem_deref
 **
 **/
char *wstk_str_wrap(char *str) {
    char *out = NULL;
    size_t len = 0;

    if(wstk_str_is_empty(str)) {
        return NULL;
    }

    len = strlen(str) + 1;
    out = wstk_mem_wrap(str, len, NULL);
    if(out) { out[len] = '\0'; }

    return out;
}

wstk_status_t wstk_str_dup2(char **out, const char *str) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;

    if(!out || !str) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *out = wstk_str_dup(str);
    if(!*out) { return WSTK_STATUS_MEM_FAIL; }

    return st;
}

wstk_status_t wstk_str_ndup2(char **out, const char *str, size_t len) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;

    if(!out || !str || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *out = wstk_str_ndup(str, len);
    if(!*out) { return WSTK_STATUS_MEM_FAIL; }

    return st;
}


/**
 **  usage: wstk_str_concat("s1", "s2", 0x0);
 **/
char *wstk_str_concat(const char *s, ...) {
    static int max_args = 32;
    size_t saved_lengths[max_args];
    size_t len = 0, slen = 0;
    char *cp = NULL, *argp = NULL, *res = NULL;
    int nargs = 0;
    va_list adummy;

    slen = (s != NULL ? strlen(s) : 0);
    len = slen;

    va_start(adummy, s);
    while((cp = va_arg(adummy, char *)) != NULL) {
        size_t cplen = strlen(cp);
        if(nargs < max_args) { saved_lengths[nargs++] = cplen; }
        len += cplen;
    }
    va_end(adummy);

    if(wstk_mem_alloc((void *)&res, len + 1, NULL) != WSTK_STATUS_SUCCESS) {
        return NULL;
    }

    cp = res;
    if(slen) {
        memcpy(cp, s, slen);
        cp += slen;
    }

    va_start(adummy, s);
    nargs = 0;
    while ((argp = va_arg(adummy, char *)) != NULL) {
        if (nargs < max_args) {
            len = saved_lengths[nargs++];
        } else {
            len = strlen(argp);
        }
        memcpy(cp, argp, len);
        cp += len;
    }
    va_end(adummy);

    *cp = 0x0;
    return res;
}

char *wstk_strstr(const char *s1, const char *s2) {
    if(!s1 || !s2) {
        return NULL;
    }

    return strstr(s1, s2);
}

char *wstk_strnstr(const char *buf, size_t buf_len, const char *str) {
    char *ptr = NULL;
    size_t str_len = 0;
    uint32_t i,j;

    if(!buf || !str || !buf_len) {
        return NULL;
    }

    str_len = strlen(str);
    for(i=0, j=0; i < buf_len; i++) {
        if(buf[i] == str[j]) {
            j++;
            if(j >= str_len) {
                break;
            }
        } else {
            j = 0;
        }
    }

    if(j >= str_len) {
        ptr = (void *)(buf + (i - (str_len - 1)));
    }
    return ptr;
}

int wstk_str_cmp(const char *s1, const char *s2) {
    if(!s1 || !s2) {
        return 1;
    }
    return strcmp(s1, s2);
}

int wstk_str_ncmp(const char *s1, const char *s2, size_t n) {
    if(!s1 || !s2) {
        return 1;
    }
    return strncmp(s1, s2, n);
}

int wstk_str_casecmp(const char *s1, const char *s2) {
    if(s1 == s2) {
        return 0;
    }

    if(!s1 || !s2) {
        return 1;
    }

    return strcasecmp(s1, s2);
}


bool wstk_str_equal(const char *s1, const char *s2, bool csensetive) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    if(csensetive) {
        if(strcmp(s1, s2) == 0) {
            return true;
        }
    } else {
        if(strcasecmp(s1, s2) == 0) {
            return true;
        }
    }

    return false;
}

wstk_status_t wstk_str_lower(char *str, size_t len) {
    int i = 0;

    if(!str || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for(i = 0; i<len; i++) {
        str[i] = l_ilower(str[i]);
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_upper(char *str, size_t len) {
    int i = 0;

    if(!str || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for(i = 0; i<len; i++) {
        str[i] = l_iupper(str[i]);
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_lower_dup(char **dst, const char *str, size_t len) {
    char *tmp = NULL;

    tmp = wstk_str_ndup(str, len);
    if(!tmp) {
        return WSTK_STATUS_MEM_FAIL;
    }

    wstk_str_lower(tmp, len);
    *dst = tmp;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_upper_dup(char **dst, const char *str, size_t len) {
    char *tmp = NULL;

    tmp = wstk_str_ndup(str, len);
    if(!tmp) {
        return WSTK_STATUS_MEM_FAIL;
    }

    wstk_str_upper(tmp, len);
    *dst = tmp;

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_hex2bin(uint8_t *dst, const char *src) {
    size_t len = wstk_str_len(src);
    size_t i = 0;

    if (!dst || !src) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    for (i=0; i < len * 2; i+=2) {
        dst[i/2]  = wstk_ch_hex(src[i]) << 4;
        dst[i/2] += wstk_ch_hex(src[i+1]);
    }

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_hex2bin_dup(uint8_t **dst, const char *src) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    size_t slen = wstk_str_len(src);
    size_t dsz = slen * 2;
    uint8_t *b = NULL;

    if (!dst || !src) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((st = wstk_mem_alloc((void *)&b, dsz, NULL)) != WSTK_STATUS_SUCCESS) {
        return st;
    }

    if((st = wstk_str_hex2bin(b, src)) != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(b);
        return st;
    }

    *dst = b;
    return st;
}

wstk_status_t wstk_str_bin2hex(char *dst, const uint8_t *src, size_t len) {
    short i, x;
    uint8_t b;

    if(!src || !len) {
        return WSTK_STATUS_FALSE;
    }

    for (x = i = 0; x < len; x++) {
        b = (src[x] >> 4) & 15;
        dst[i++] = b + (b > 9 ? 'a' - 10 : '0');
        b = src[x] & 15;
        dst[i++] = b + (b > 9 ? 'a' - 10 : '0');
    }

    dst[i] = '\0';
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_str_bin2hex_dup(char **dst, const uint8_t *src, size_t len) {
    wstk_status_t st;
    char *t = NULL;

    if(!src || !len) {
        return WSTK_STATUS_FALSE;
    }

    if((st =wstk_mem_alloc((void *)&t, (len * 2) + 1, NULL)) != WSTK_STATUS_SUCCESS) {
        return st;
    }

    if((st = wstk_str_bin2hex(t, src, len)) != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(t);
        return st;
    }

    *dst = t;
    return st;
}

uint32_t wstk_str_separate(char *str, char delim, char **array, size_t arraylen) {
    uint32_t result = 0;

    if(!str || !array || !arraylen) {
        return 0;
    }

    memset(array, 0, arraylen * sizeof(*array));

    if(delim == ' ') {
        result = separate_string_blank_delim(str, array, arraylen);
    } else {
        result = separate_string_char_delim(str, delim, array, arraylen);
    }

    return result;
}

char *wstk_str_quote_shell_arg(const char *str, size_t str_len) {
    wstk_mbuf_t *tbuf = NULL;
    char *dest = NULL;
    size_t i, n = 0;

    if(!str || !str_len) {
        return NULL;
    }

    if(wstk_mbuf_alloc(&tbuf, str_len * 2) != WSTK_STATUS_SUCCESS) {
        return NULL;
    }

#ifdef WSTK_OS_WIN
    wstk_mbuf_write_u8(tbuf, '"');
#else
    wstk_mbuf_write_u8(tbuf, '\'');
#endif

    for (i = 0; i < str_len; i++) {
        switch (str[i]) {
#ifdef WSTK_OS_WIN
            case '"':
            case '%':
                wstk_mbuf_write_u8(tbuf, ' ');
                break;
#else
            case '\'':
                wstk_mbuf_write_u8(tbuf, '\'');
                wstk_mbuf_write_u8(tbuf, '\\');
                wstk_mbuf_write_u8(tbuf, '\'');
                wstk_mbuf_write_u8(tbuf, '\'');
                break;
#endif
            default:
                wstk_mbuf_write_u8(tbuf, str[i]);
        }
    }

#ifdef WSTK_OS_WIN
    wstk_mbuf_write_u8(tbuf, '"');
#else
    wstk_mbuf_write_u8(tbuf, '\'');
#endif
    wstk_mbuf_write_u8(tbuf, 0x0);

    tbuf->pos = 0;
    if(wstk_mbuf_strdup(tbuf, &dest, tbuf->end) != WSTK_STATUS_SUCCESS) {
        dest = NULL;
    }
    wstk_mem_deref(tbuf);

    return dest;
}

bool wstk_str_match(const char *str, size_t str_len, const char *search, size_t search_len) {
    size_t i;

    if(!str || !str_len) {
        return NULL;
    }

    for(i = 0; (i < search_len) && (i < str_len); i++) {
        if(str[i] != search[i]) {
            return false;
        }
    }

    if(i == search_len) {
        return true;
    }

    return false;
}

char *wstk_str_replace(const char *str, size_t str_len, const char *search, const char *replace) {
    wstk_mbuf_t *tbuf = NULL;
    size_t search_len = wstk_str_len(search);
    size_t replace_len = wstk_str_len(replace);
    size_t i, n;
    char *dest = NULL;

    if(!str || !str_len || !search || !replace) {
        return NULL;
    }

    if(wstk_mbuf_alloc(&tbuf, str_len * 2) != WSTK_STATUS_SUCCESS) {
        return NULL;
    }

    for(i = 0; i < str_len; i++) {
        if(wstk_str_match(str + i, str_len - i, search, search_len)) {
            for(n = 0; n < replace_len; n++) {
                wstk_mbuf_write_u8(tbuf, replace[n]);
            }
            i += search_len - 1;
        } else {
            wstk_mbuf_write_u8(tbuf, str[i]);
        }
    }
    wstk_mbuf_write_u8(tbuf, 0x0);

    tbuf->pos = 0;
    if(wstk_mbuf_strdup(tbuf, &dest, tbuf->end) != WSTK_STATUS_SUCCESS) {
        dest = NULL;
    }
    wstk_mem_deref(tbuf);

    return dest;
}

bool wstk_str_atob(const char *str) {
    if(wstk_str_is_empty(str)) {
        return false;
    }
    if(!wstk_str_casecmp(str, "0")) {
        return false;
    } else if (!wstk_str_casecmp(str, "1")) {
        return true;
    } else if (!wstk_str_casecmp(str, "false")) {
        return false;
    } else if (!wstk_str_casecmp(str, "true")) {
        return true;
    } else if (!wstk_str_casecmp(str, "disable")) {
        return false;
    } else if (!wstk_str_casecmp(str, "enable")) {
        return true;
    } else if (!wstk_str_casecmp(str, "off")) {
        return false;
    } else if (!wstk_str_casecmp(str, "on")) {
        return true;
    } else if (!wstk_str_casecmp(str, "no")) {
        return false;
    } else if (!wstk_str_casecmp(str, "yes")) {
        return true;
    }

    return false;
}

int wstk_str_atoi(const char *str) {
    if(!str) { return 0; }
    return atoi(str);
}

long wstk_str_atol(const char *str) {
    if(!str) { return 0; }
    return atol(str);
}

double wstk_str_atof(const char *str) {
    if(!str) { return 0.0; }
    return atof(str);
}
