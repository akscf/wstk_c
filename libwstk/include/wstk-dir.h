/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_DIR_H
#define WSTK_DIR_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char        *name;  // target name
    char        *path;  // absolute path
    time_t      mtime;
    size_t      size;
    bool        directory;
} wstk_dir_entry_t;

typedef wstk_status_t (*wstk_dir_list_handler_t)(wstk_dir_entry_t *dirent, void *udata);

bool wstk_dir_exists(const char *dirname);
wstk_status_t wstk_dir_create(const char *dirname, bool recursive);

wstk_status_t wstk_dir_list(const char *base, char separator, wstk_dir_list_handler_t handler, void *udata);


#ifdef __cplusplus
}
#endif
#endif
