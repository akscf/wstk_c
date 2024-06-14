/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_STR_H
#define WSTK_STR_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline bool wstk_str_is_empty(const char *s) {
    return !s || s[0] == '\0';
}

uint8_t wstk_ch_hex(char ch);
size_t wstk_str_len(const char *s);

char *wstk_str_dup(const char *s);
char *wstk_str_ndup(const char *s, size_t len);
char *wstk_str_concat(const char *s, ...);
char *wstk_strstr(const char *s1, const char *s2);
char *wstk_strnstr(const char *buf, size_t buf_len, const char *str);

char *wstk_str_wrap(char *str);

char *wstk_str_quote_shell_arg(const char *str, size_t str_len);
char *wstk_str_replace(const char *str, size_t str_len, const char *search, const char *replace);
bool wstk_str_match(const char *str, size_t str_len, const char *search, size_t search_len);
uint32_t wstk_str_separate(char *str, char delim, char **array, size_t arraylen);

int wstk_str_cmp(const char *s1, const char *s2);
int wstk_str_ncmp(const char *s1, const char *s2, size_t n);
int wstk_str_casecmp(const char *s1, const char *s2);
bool wstk_str_equal(const char *s1, const char *s2, bool csensetive);

bool wstk_str_atob(const char *str);
int wstk_str_atoi(const char *str);
long wstk_str_atol(const char *str);
double wstk_str_atof(const char *str);

wstk_status_t wstk_str_dup2(char **out, const char *str);
wstk_status_t wstk_str_ndup2(char **out, const char *str, size_t len);

wstk_status_t wstk_str_lower(char *str, size_t len);
wstk_status_t wstk_str_upper(char *str, size_t len);
wstk_status_t wstk_str_lower_dup(char **dst, const char *str, size_t len);
wstk_status_t wstk_str_upper_dup(char **dst, const char *str, size_t len);

wstk_status_t wstk_str_hex2bin(uint8_t *dst, const char *src);
wstk_status_t wstk_str_hex2bin_dup(uint8_t **dst, const char *src);
wstk_status_t wstk_str_bin2hex(char *dst, const uint8_t *src, size_t len);
wstk_status_t wstk_str_bin2hex_dup(char **dst, const uint8_t *src, size_t len);


#ifdef __cplusplus
}
#endif
#endif
