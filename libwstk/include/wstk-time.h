/**
 *
 * (C)2019 aks
 **/
#ifndef WSTK_TIME_H
#define WSTK_TIME_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_TIME_RFC822_STRING_SIZE 40
#define WSTK_TIME_JSON_STRING_SIZE   30
#define WSTK_TIME_STRING_SIZE        20

/* unixtime in sconds */
uint32_t wstk_time_epoch_now();

/* unixtime miroaecs */
uint64_t wstk_time_micro_now();

wstk_status_t wstk_localtime(time_t time, struct tm *tm);
wstk_status_t wstk_gmtime(time_t time, struct tm *tm);

/* Wed, 21 Oct 2015 07:28:00 GMT */
wstk_status_t wstk_time_to_str_rfc822(time_t time, char *buf, size_t buf_len);

/* DD-MM-YYYY HH:MM:SS */
wstk_status_t wstk_time_to_str(time_t time, char *buf, size_t buf_len);
wstk_status_t wstk_time_from_str(time_t *time, char *str, size_t str_len);

/* yyyy-MM-dd'T'HH:mm:ss.SSS'Z' */
wstk_status_t wstk_time_to_str_json(time_t time, char *buf, size_t buf_len);
wstk_status_t wstk_time_from_str_json(time_t *time, char *str, size_t str_len);

#ifdef __cplusplus
}
#endif
#endif
