/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_WIN_H
#define WSTK_WIN_H

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef WSTK_NET_HAVE_INET6
#undef WSTK_NET_HAVE_UN

#ifndef SHUT_RD
 #define SHUT_RD    SD_RECEIVE
#endif
#ifndef SHUT_WR
 #define SHUT_WR    SD_SEND
#endif
#ifndef SHUT_RDWR
 #define SHUT_RDWR  SD_BOTH
#endif

#ifndef EWOULDBLOCK
 #define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
 #define EINPROGRESS WSAEINPROGRESS
#endif


#define strcasecmp(s1, s2) _stricmp(s1, s2)


#define WSTK_PATH_DELIMITER     '/'
#define WSTK_MAXNAMLEN          255
// #define WSTK_THREAD_STACK_SIZE  16384

#ifdef __cplusplus
}
#endif
#endif
