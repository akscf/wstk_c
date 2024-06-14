/**
 **
 ** (C)2018 aks
 **/
#include <wstk-pid.h>
#include <wstk-log.h>

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
pid_t wstk_pid_read(const char *filename) {
    FILE *f = NULL;
    pid_t pid;

    if(!(f = fopen(filename, "r"))) {
        return 0;
    }
    int rd = fscanf(f, "%d", &pid);
    fclose(f);

    return pid;
}

wstk_status_t wstk_pid_write(const char *filename, pid_t pid) {
    FILE *f = NULL;
    int fd;

    if(((fd = open(filename, O_RDWR | O_CREAT, 0644)) == -1) || ((f = fdopen(fd, "r+")) == NULL)) {
        return WSTK_STATUS_FALSE;
    }
    if(!fprintf(f, "%d\n", pid)) {
        close(fd);
        return WSTK_STATUS_FALSE;
    }

    fflush(f);
    close(fd);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pid_delete(const char *filename) {
    return unlink(filename) == 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE;
}
