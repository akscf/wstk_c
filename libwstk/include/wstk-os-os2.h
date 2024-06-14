/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_OS2_H
#define WSTK_OS2_H

#define INCL_BASE
#define INCL_DOS
#include <os2.h>
#include <process.h>

#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef WSTK_NET_HAVE_INET6
#undef WSTK_NET_HAVE_UN

/* soc_shutdown */
#ifndef SHUT_RD
 #define SHUT_RD 0
#endif
#ifndef SHUT_WR
 #define SHUT_WR 1
#endif
#ifndef SHUT_RDWR
 #define SHUT_RDWR 2
#endif

// -----------------------------------------------------------------------
#ifndef __int8_t_defined
#define __int8_t_defined
#if defined(_CHAR_IS_SIGNED)
 typedef char                    int8_t;
#elif defined(__STDC__)
 typedef signed char             int8_t;
#else
 typedef char                    int8_t;
#endif
 typedef signed short int        int16_t;
 typedef signed int              int32_t;
 typedef signed long long int    int64_t;
#endif // __int8_t_defined

#ifndef __uint32_t_defined
#define __uint32_t_defined
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long int  uint64_t;
#endif // __uint32_t_defined

#ifndef bool
typedef uint8_t                 bool;
#endif // bool

// stdint.h
# if __WORDSIZE == 64
#  define __INT64_C(c)  c ## L
#  define __UINT64_C(c) c ## UL
# else
#  define __INT64_C(c)  c ## LL
#  define __UINT64_C(c) c ## ULL
# endif

/* Types for `void *' pointers.  */
#if __WORDSIZE == 64
# ifndef __intptr_t_defined
typedef long int                intptr_t;
#  define __intptr_t_defined
# endif
typedef unsigned long int       uintptr_t;
#else
# ifndef __intptr_t_defined
typedef int                     intptr_t;
#  define __intptr_t_defined
# endif
typedef unsigned int            uintptr_t;
#endif

/* Minimum of signed integral types.  */
# define INT8_MIN               (-128)
# define INT16_MIN              (-32767-1)
# define INT32_MIN              (-2147483647-1)
# define INT64_MIN              (-__INT64_C(9223372036854775807)-1)
/* Maximum of signed integral types.  */
# define INT8_MAX               (127)
# define INT16_MAX              (32767)
# define INT32_MAX              (2147483647)
# define INT64_MAX              (__INT64_C(9223372036854775807))

/* Maximum of unsigned integral types.  */
# define UINT8_MAX              (255)
# define UINT16_MAX             (65535)
# define UINT32_MAX             (4294967295U)
# define UINT64_MAX             (__UINT64_C(18446744073709551615))

// -----------------------------------------------------------------------
#define _ERRNO
int *_errno (void);
#define errno (*_errno ())


typedef uint32_t socklen_t;


#define strcasecmp stricmp


#define WSTK_PATH_DELIMITER     '/'
#define WSTK_MAXNAMLEN          255
#define WSTK_THREAD_STACK_SIZE  32768



#ifdef __cplusplus
}
#endif
#endif
