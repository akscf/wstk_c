/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_NIX_H
#define WSTK_NIX_H

#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <syslog.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>

#include <dirent.h>
#include <pthread.h>

#ifdef WSTK_OS_SUNOS
#include <sys/filio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_NET_HAVE_INET6
#define WSTK_NET_HAVE_UN

#define WSTK_PATH_DELIMITER     '/'
#define WSTK_MAXNAMLEN          255
// #define WSTK_THREAD_STACK_SIZE  16384*4




#ifdef __cplusplus
}
#endif
#endif
