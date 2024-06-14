/**
 **
 ** (C)2019 aks
 **/
#include <wstk-time.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mutex.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-pl.h>
#include <wstk-regex.h>

static const char *dayv[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *monv[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static wstk_mutex_t *time_mutex = NULL;

#define LOCK() if(time_mutex) wstk_mutex_lock(time_mutex);
#define UNLOCK() if(time_mutex) wstk_mutex_unlock(time_mutex);

wstk_status_t wstk_pvt_time_init() {
    wstk_status_t st = WSTK_STATUS_SUCCESS;

    if(!time_mutex) {
        st = wstk_mutex_create(&time_mutex);
    }

    return st;
}

wstk_status_t wstk_pvt_time_shutdown() {
    if(time_mutex) {
        time_mutex = wstk_mem_deref(time_mutex);
    }
    return WSTK_STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Get time
 *
 * @return unix time in seconds
 **/
uint32_t wstk_time_epoch_now() {
    return time(NULL);
}

/**
 * Get time
 *
 * @return unix time in micro seconds
 **/
uint64_t wstk_time_micro_now() {
    struct timeval tv = { 0 };

    gettimeofday(&tv,NULL);
    return (1000000 * tv.tv_sec + tv.tv_usec);
}

/**
 * Thread safe version
 *
 * @param time    - the time
 * @param tm      - struct tm, result
 *
 * @return succes or error
 **/
wstk_status_t wstk_localtime(time_t time, struct tm *tm) {
    wstk_status_t st = WSTK_STATUS_FALSE;
    const struct tm *ltm;

    if(!tm) {
        return WSTK_STATUS_INVALID_PARAM;
    }

#ifdef WSTK_HAVE_LOCALTIME_R
    if(localtime_r(&time, tm) != NULL) {
        st = WSTK_STATUS_SUCCESS;
    }
#else
    LOCK();
    if((ltm = localtime(&time)) != NULL) {
        memcpy(tm, ltm, sizeof(struct tm));
        st = WSTK_STATUS_SUCCESS;
    }
    UNLOCK();
#endif

    return st;
}

/**
 * Thread safe version
 *
 * @param time    - the time
 * @param tm      - struct tm, result
 *
 * @return succes or error
 **/
wstk_status_t wstk_gmtime(time_t time, struct tm *tm) {
    wstk_status_t st = WSTK_STATUS_FALSE;
    const struct tm *ltm;

    if(!tm) {
        return WSTK_STATUS_INVALID_PARAM;
    }

#ifdef WSTK_HAVE_GMTIME_R
    if(gmtime_r(&time, tm) != NULL) {
        st = WSTK_STATUS_SUCCESS;
    }
#else
    LOCK();
    if((ltm = gmtime(&time)) != NULL) {
        memcpy(tm, ltm, sizeof(struct tm));
        st = WSTK_STATUS_SUCCESS;
    }
    UNLOCK();
#endif

    return st;
}

/**
 * Time to str RFC822
 * Wed, 21 Oct 2015 07:28:00 GMT
 *
 * @param time      - epoch time or 0 for now
 * @param buf       - buffer to output
 * @param buf_len   - buffer length
 *
 * @return succes or error
 **/
wstk_status_t wstk_time_to_str_rfc822(time_t time, char *buf, size_t buf_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct tm tm = { 0 };
    int err = 0;

    if(!buf || !buf_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!time) {
        time = wstk_time_epoch_now();
    }

    if((status = wstk_gmtime(time, &tm)) == WSTK_STATUS_SUCCESS) {
        err = wstk_snprintf(buf, buf_len, "%s, %02u %s %u %02u:%02u:%02u GMT",
                dayv[MIN((unsigned)tm.tm_wday, ARRAY_SIZE(dayv)-1)],
                tm.tm_mday,
                monv[MIN((unsigned)tm.tm_mon, ARRAY_SIZE(monv)-1)],
                tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec
        );
        if(err < 0 ) {
            status = WSTK_STATUS_FALSE;
        }
    }

    return status;
}

/**
 * Time to string
 * DD-MM-YYYY HH:MM:SS
 *
 * @param time      - epoch time or 0 for now
 * @param buf       - buffer to output
 * @param buf_len   - buffer length
 *
 * @return succes or error
 **/
wstk_status_t wstk_time_to_str(time_t time, char *buf, size_t buf_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct tm tm = { 0 };
    int err = 0;

    if(!buf || !buf_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!time) {
        time = wstk_time_epoch_now();
    }

    if((status = wstk_localtime(time, &tm)) == WSTK_STATUS_SUCCESS) {
        err = wstk_snprintf(buf, buf_len, "%02d-%02d-%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if(err < 0 ) {
            status = WSTK_STATUS_FALSE;
        }
    }

    return status;
}

/**
 * recovery time from string
 * DD-MM-YYYY HH:MM:SS
 *
 * @param time      - recovered time
 * @param str       - time in str
 * @param str_len   - str length
 *
 * @return succes or error
 **/
wstk_status_t wstk_time_from_str(time_t *time, char *str, size_t str_len) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    wstk_pl_t pl1,pl2,pl3,pl4,pl5,pl6;
    struct tm lt = {0};

    if(!time || !str || !str_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    st = wstk_regex(str, str_len, "[0-9]2\\-[0-9]2\\-[0-9]4\\ [0-9]2\\:[0-9]2\\:[0-9]2", &pl1, &pl2, &pl3, &pl4, &pl5, &pl6);
    if(st != WSTK_STATUS_SUCCESS) {
        if(st == WSTK_STATUS_NOT_FOUND) { st = WSTK_STATUS_INVALID_VALUE; }
        return st;
    }

    lt.tm_isdst = -1;
    lt.tm_mday  = wstk_pl_u32(&pl1);
    lt.tm_mon   = wstk_pl_u32(&pl2);
    lt.tm_year  = wstk_pl_u32(&pl3);
    lt.tm_hour  = wstk_pl_u32(&pl4);
    lt.tm_min   = wstk_pl_u32(&pl5);
    lt.tm_sec   = wstk_pl_u32(&pl6);

    if(lt.tm_year > 0) { lt.tm_year -= 1900; }
    if(lt.tm_mon > 0)  { lt.tm_mon -= 1; }

    *time = mktime(&lt);
    return st;
}

/**
 * Time to json format
 * yyyy-MM-dd'T'HH:mm:ss.SSS'Z'
 *
 * @param time      - epoch time or 0 for now
 * @param buf       - buffer to output
 * @param buf_len   - buffer length
 *
 * @return succes or error
 **/
wstk_status_t wstk_time_to_str_json(time_t time, char *buf, size_t buf_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct tm tm = { 0 };
    int err = 0;

    if(!buf || !buf_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!time) {
        time = wstk_time_epoch_now();
    }

    if((status = wstk_localtime(time, &tm)) == WSTK_STATUS_SUCCESS) {
        err = wstk_snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if(err < 0 ) {
            status = WSTK_STATUS_FALSE;
        }
    }

    return status;
}

/**
 * Recovery time from json format
 * yyyy-MM-dd'T'HH:mm:ss.SSS'Z'
 *
 * @param time      - epoch time or 0 for now
 * @param str       - time in str
 * @param str_len   - str length
 *
 * @return succes or error
 **/
wstk_status_t wstk_time_from_str_json(time_t *time, char *str, size_t str_len) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;
    wstk_pl_t pl1,pl2,pl3,pl4,pl5,pl6,pl7;
    struct tm lt = {0};

    if(!time || !str || !str_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    st = wstk_regex(str, str_len, "[0-9]4\\-[0-9]2\\-[0-9]2T[0-9]2\\:[0-9]2\\:[0-9]2\\.[0-9]3Z", &pl1, &pl2, &pl3, &pl4, &pl5, &pl6, &pl7);
    if(st != WSTK_STATUS_SUCCESS) {
        if(st == WSTK_STATUS_NOT_FOUND) { st = WSTK_STATUS_INVALID_VALUE; }
        return st;
    }

    lt.tm_isdst = -1;
    lt.tm_year  = wstk_pl_u32(&pl1);
    lt.tm_mon   = wstk_pl_u32(&pl2);
    lt.tm_mday  = wstk_pl_u32(&pl3);
    lt.tm_hour  = wstk_pl_u32(&pl4);
    lt.tm_min   = wstk_pl_u32(&pl5);
    lt.tm_sec   = wstk_pl_u32(&pl6);

    if(lt.tm_year > 0) lt.tm_year -= 1900;
    if(lt.tm_mon > 0) lt.tm_mon -= 1;

    *time = mktime(&lt);
    return st;
}
