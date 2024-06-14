/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_PID_H
#define WSTK_PID_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

pid_t wstk_pid_read(const char *filename);
wstk_status_t wstk_pid_write(const char *filename, pid_t pid);
wstk_status_t wstk_pid_delete(const char *filename);


#ifdef __cplusplus
}
#endif
#endif
