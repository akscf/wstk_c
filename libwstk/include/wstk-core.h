/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_CORE_H
#define WSTK_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OS/2 */
#if defined(WSTK_OS_OS2)
 #define WSTK_OS_NAME "os2"
 #define WSTK_BUILTIN_FPC
 #include "wstk-os-os2.h"

/* Windows   */
#elif defined(WSTK_OS_WIN)
 #define WSTK_OS_NAME "windows"
 #define WSTK_BUILTIN_FPC
 #include "wstk-os-win.h"

/* FreeBSD, OpenBSD, NetBSD  */
#elif defined(WSTK_OS_FREEBSD) || defined(WSTK_OS_OPENBSD) || defined(WSTK_OS_NETBSD)
 #define WSTK_OS_NAME "freebsd"
 #define WSTK_HAVE_SYSLOG
 #define WSTK_HAVE_KQUEUE
 #define WSTK_HAVE_GMTIME_R
 #define WSTK_HAVE_LOCALTIME_R
 #define WSTK_HAVE_ATOMIC
 #include "wstk-os-nix.h"

/* Linux */
#elif defined(WSTK_OS_LINUX)
 #define WSTK_OS_NAME "linux"
 #define WSTK_HAVE_SYSLOG
 #define WSTK_HAVE_EPOLL
 #define WSTK_HAVE_GMTIME_R
 #define WSTK_HAVE_LOCALTIME_R
 #define WSTK_HAVE_ATOMIC
 #include "wstk-os-nix.h"

/* Solaris */
#elif defined(WSTK_OS_SUNOS)
 #define WSTK_OS_NAME "solaris"
 #define WSTK_HAVE_SYSLOG
 #define WSTK_HAVE_GMTIME_R
 #define WSTK_HAVE_LOCALTIME_R
 #define WSTK_BUILTIN_FPC
 #include "wstk-os-nix.h"

/* Darwin */
#elif defined(WSTK_OS_DARWIN)
 #define WSTK_OS_NAME "darwin"
 #define WSTK_HAVE_SYSLOG
 #include "wstk-os-nix.h"
#endif


#ifndef WSTK_OS_NAME
 #error "Unsuppoted OS"
#endif


#if (defined(__i386__) || defined(__x86_64__))
#define WSTK_BREAKPOINT __asm__("int $0x03")
#elif defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
#define WSTK_BREAKPOINT __builtin_debugtrap()
#endif
#endif

#ifndef WSTK_BREAKPOINT
 #define WSTK_BREAKPOINT
#endif

#ifdef WSTK_SHARED
#ifdef WSTK_OS_WIN
 #ifdef WSTK_BUILD
  #define WSTK_API __declspec(dllexport)
 #else
  #define WSTK_API __declspec(dllimport)
 #endif
#else
 #define WSTK_API __attribute__ ((visibility ("default")))
#endif
#else
 #define WSTK_API
#endif


/* debug */
#include <assert.h>
#include <wstk-debug.h>

/* sys types */
#include <wstk-types.h>
#include <wstk-atomic.h>

#define wstk_goto_status(_status, _label) status = _status; goto _label

#define WSTK_VERSION_STR  "1.2.0a"
#define WSTK_VERSION_MAJ  1
#define WSTK_VERSION_MIN  2
#define WSTK_VERSION_REV  0

typedef enum {
    WSTK_STATUS_SUCCESS = 0,        // 0
    WSTK_STATUS_FALSE,              // 1
    WSTK_STATUS_NOT_FOUND,          // 2
    WSTK_STATUS_ALREADY_EXISTS,     // 3
    WSTK_STATUS_OUTDATE,            // 4
    WSTK_STATUS_NULL_PTR,           // 5
    WSTK_STATUS_INVALID_PARAM,      // 6
    WSTK_STATUS_INVALID_VALUE,      // 7
    WSTK_STATUS_UNSUPPORTED,        // 8
    WSTK_STATUS_NOT_IMPL,           // 9
    WSTK_STATUS_OUTOFRANGE,         // 10
    WSTK_STATUS_TIMEOUT,            // 11
    WSTK_STATUS_NODATA,             // 12
    WSTK_STATUS_NOSPACE,            // 13
    WSTK_STATUS_BUSY,               // 14
    WSTK_STATUS_MEM_FAIL,           // 15
    WSTK_STATUS_DESTROYED,          // 16
    WSTK_STATUS_CONN_FAIL,          // 17
    WSTK_STATUS_BIND_FAIL,          // 18
    WSTK_STATUS_LOCK_FAIL,          // 19
    WSTK_STATUS_CONN_DISCON,        // 20
    WSTK_STATUS_CONN_EXPIRE,        // 21
    WSTK_STATUS_BAD_REQUEST,        // 22
    WSTK_STATUS_ALREADY_OPENED      // 23
} wstk_status_t;


/* wstk-core.c */
wstk_status_t wstk_core_init();
wstk_status_t wstk_core_shutdown();

/* wstk-common.c */
void wstk_dbg_hexdump(const void *p, size_t len);



#ifdef __cplusplus
}
#endif
#endif
