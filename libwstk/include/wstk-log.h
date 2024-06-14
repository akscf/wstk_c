/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_LOG_H
#define WSTK_LOG_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WSTK_LOG_SYSLOG,
    WSTK_LOG_STDERR,
    WSTK_LOG_FILE,
} wstk_log_mode_e;

wstk_status_t wstk_log_configure(wstk_log_mode_e mode, char *name);

void wstk_log_debug(const char *fmt, ...);
void wstk_log_vdebug(const char *fmt, va_list ap);

void wstk_log_notice(const char *fmt, ...);
void wstk_log_vnotice(const char *fmt, va_list ap);

void wstk_log_error(const char *fmt, ...);
void wstk_log_verror(const char *fmt, va_list ap);

void wstk_log_warn(const char *fmt, ...);
void wstk_log_vwarn(const char *fmt, va_list ap);


#if defined(WSTK_OS_FREEBSD) || defined(WSTK_OS_OPENBSD) || defined(WSTK_OS_DARWIN)
 #define log_debug(fmt, ...) do{wstk_log_notice("DEBUG [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#else
 #define log_debug(fmt, ...) do{wstk_log_debug("DEBUG [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#endif

#define log_notice(fmt, ...) do{wstk_log_notice("NOTICE [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_error(fmt, ...) do{wstk_log_error("ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_warn(fmt, ...) do{wstk_log_warn("WARN [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)

#define log_mem_fail_goto_status(_label) do{wstk_log_error("FAIL [%s:%d]: mem fail!\n", __FILE__, __LINE__); status = WSTK_STATUS_MEM_FAIL; goto _label; } while (0)


#define WSTK_DBG_PRINT(fmt, ...) do{wstk_log_debug("DEBUG [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)



#ifdef __cplusplus
}
#endif
#endif
