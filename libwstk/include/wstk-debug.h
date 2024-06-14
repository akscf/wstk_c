/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_DEBUG_H
#define WSTK_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_UDP_LOG_ERRORS
#define WSTK_TCP_LOG_ERRORS

#ifdef WSTK_DEBUG__MEM
 #define WSTK_MEM_DEBUG
#endif

#ifdef WSTK_DEBUG__DS
 #define WSTK_LIST_DEBUG
 #define WSTK_HASTABLE_DEBUG
 #define WSTK_QUEUE_DEBUG
#endif

#ifdef WSTK_DEBUG__MT
 #define WSTK_DLO_DEBUG
 #define WSTK_MUTEX_DEBUG
 #define WSTK_THREAD_DEBUG
 #define WSTK_WORKER_DEBUG
#endif

#ifdef WSTK_DEBUG__NET
 #define WSTK_SOCK_DEBUG
 #define WSTK_UDP_DEBUG
 #define WSTK_TCP_DEBUG

 #define WSTK_TCP_SRV_DEBUG
 #define WSTK_UDP_SRV_DEBUG

 #define WSTK_POLL_DEBUG

 #define WSTK_HTTP_MSG_DEBUG
 #define WSTK_HTTPD_DEBUG

 #define WSTK_SERVLET_JSONRPC_DEBUG
 #define WSTK_SERVLET_WEBSOCK_DEBUG
 #define WSTK_SERVLET_UPLOAD_DEBUG
#endif



#ifdef __cplusplus
}
#endif
#endif
