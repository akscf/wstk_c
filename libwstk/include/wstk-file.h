/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_FILE_H
#define WSTK_FILE_H
#include <wstk-core.h>
#include <wstk-pl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_FILE_DEBUG

typedef struct {
    int32_t     mode;
    int32_t     uid;
    int32_t     gid;
    size_t      size;
    time_t      ctime;
    time_t      mtime;
    time_t      atime;
    bool        isdir;
} wstk_file_meta_t;

typedef struct {
    FILE        *fh;
    size_t      fsize;
} wstk_file_t;

bool wstk_file_exists(const char *filename);
wstk_status_t wstk_file_delete(const char *filename);

wstk_status_t wstk_file_get_meta(const char *filename, wstk_file_meta_t *meta);
wstk_status_t wstk_file_get_size(const char *filename, size_t *size);

wstk_status_t wstk_file_content_read(const char *filename, wstk_mbuf_t *mbuf, size_t offset);
wstk_status_t wstk_file_content_write(const char *filename, wstk_mbuf_t *mbuf, size_t offset);

wstk_status_t wstk_file_open(wstk_file_t *file, const char *filename, const char *mode);
wstk_status_t wstk_file_close(wstk_file_t *file);
wstk_status_t wstk_file_seek(wstk_file_t *file, size_t ofs);
wstk_status_t wstk_file_tell(wstk_file_t *file, size_t *pos);
wstk_status_t wstk_file_read(wstk_file_t *file, wstk_mbuf_t *mbuf);
wstk_status_t wstk_file_write(wstk_file_t *file, wstk_mbuf_t *mbuf);

wstk_status_t wstk_file_name_concat(char **path, const char *dir, const char *file, char separator);
wstk_status_t wstk_file_name_extract(wstk_pl_t *fname, char *path, size_t path_len, char delimeter);



#ifdef __cplusplus
}
#endif
#endif
