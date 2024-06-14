/**
 **
 ** (C)2024 aks
 **/
#include <wstk-daemon.h>
#include <wstk-log.h>

#ifdef WSTK_OS_WIN
wstk_status_t wstk_demonize(char *home) {
    return WSTK_STATUS_UNSUPPORTED;
}
#else
wstk_status_t wstk_demonize(char *home) {
    pid_t pid;

    pid = fork();
    if(pid < 0) {
        return WSTK_STATUS_FALSE;
    } else if (pid > 0) {
        exit(0);
    }

    if(setsid() < 0) {
        return WSTK_STATUS_FALSE;
    }
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if(pid < 0) {
        return WSTK_STATUS_FALSE;
    } else if(pid > 0) {
        exit(0);
    }

    if(chdir(home ? home : "/") < 0) {
        return WSTK_STATUS_FALSE;
    }
    umask(0);

    if(freopen("/dev/null", "r", stdin) == NULL) {
        return WSTK_STATUS_FALSE;
    }
    if(freopen("/dev/null", "r", stdout) == NULL) {
        return WSTK_STATUS_FALSE;
    }
    if(freopen("/dev/null", "r", stderr) == NULL) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}
#endif // WSTK_OS_WIN
