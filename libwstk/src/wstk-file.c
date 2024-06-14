/**
 **
 ** (C)2019 aks
 **/
#include <wstk-file.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-fmt.h>
#include <wstk-mbuf.h>
#include <wstk-pl.h>


static size_t file_get_size(const char *filename) {
    struct stat st = {0};

    if(stat(filename, &st) == -1) {
        return 0;
    }

    return (size_t) st.st_size;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Check file on exist
 *
 * @param filename - the file name
 *
 * @return true/false
 **/
bool wstk_file_exists(const char *filename) {
    struct stat st = { 0 };

    if(!filename) {
        return false;
    }

    if(stat(filename, &st) == -1) {
        return false;
    }

    return(st.st_mode & S_IFREG ? true : false);
}

/**
 * Delete file by name
 *
 * @param filename - the file name
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_delete(const char *filename) {
    if(!filename) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(remove(filename) == -1) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}

/**
 * Get file metadate (size, time, so on)
 *
 * @param filename  - the file name
 * @param meta      - the reuslt
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_get_meta(const char *filename, wstk_file_meta_t *meta) {
    struct stat st = { 0 };

    if(!filename || !meta) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(stat(filename, &st) == -1) {
        return WSTK_STATUS_FALSE;
    }

    meta->size = st.st_size;
    meta->mode = st.st_mode;
    meta->uid = st.st_uid;
    meta->gid = st.st_gid;
    meta->ctime = st.st_ctime;
    meta->mtime = st.st_mtime;
    meta->atime = st.st_atime;
    meta->isdir = S_ISDIR(st.st_mode);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Get fiee size
 *
 * @param filename  - the file name
 * @param size      - the reuslt
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_get_size(const char *filename, size_t *size) {
    if(!filename || !size) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *size = file_get_size(filename);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Read file content into mbuf
 *
 * @param filename  - the file name
 * @param mbuf      - prepared buffer
 * @param offset    - intial file offset
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_content_read(const char *filename, wstk_mbuf_t *mbuf, size_t offset) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    FILE *f = NULL;
    char *ptr = NULL;
    size_t fsz = 0, rsz = 0;

    if(!filename || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    fsz = file_get_size(filename);
    if(!fsz) {
        return WSTK_STATUS_NODATA;
    }

    if(offset > fsz) {
        return WSTK_STATUS_OUTOFRANGE;
    }

    rsz = (fsz - offset);
    if(!rsz) {
        return WSTK_STATUS_NODATA;
    }
    if(rsz > wstk_mbuf_space(mbuf)) {
        rsz = wstk_mbuf_space(mbuf);
    }

    if((f = fopen(filename, "rb")) == NULL) {
#ifdef WSTK_FILE_DEBUG
        log_error("Couldn't open file");
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(offset > 0) {
        fseek(f, offset, SEEK_SET);
    }

    ptr = (char *)wstk_mbuf_buf(mbuf);
    rsz = fread(ptr, 1,rsz, f);
    if(rsz >= 0) {
        wstk_mbuf_set_pos(mbuf, rsz);
        wstk_mbuf_set_end(mbuf, mbuf->pos);
    } else {
        status = WSTK_STATUS_FALSE;
    }
out:
    if(f) {
        fclose(f);
    }
    return status;
}

/**
 * Write file content into mbuf
 *
 * @param filename  - the file name
 * @param mbuf      - buffer with content
 * @param offset    - intial file offset
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_content_write(const char *filename, wstk_mbuf_t *mbuf, size_t offset) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *ptr = NULL;
    FILE *f = NULL;
    size_t fsz=0, wsz=0;

    if(!filename || !mbuf) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(offset > 0) {
        fsz = file_get_size(filename);
        if(offset > fsz) {
            offset = fsz;
        }
    }

    if((f = fopen(filename, "wb")) == NULL) {
#ifdef WSTK_FILE_DEBUG
        log_error("Couldn't open file");
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(offset > 0) {
        fseek(f, offset, SEEK_SET );
    }

    wsz = wstk_mbuf_end(mbuf);
    if(!wsz) {
        return WSTK_STATUS_FALSE;
    }

    ptr = (char *)wstk_mbuf_buf(mbuf);
    wsz = fwrite(ptr, 1, wsz, f);
    if(wsz >= 0) {
        wstk_mbuf_set_pos(mbuf, wsz);
    } else {
        status = WSTK_STATUS_FALSE;
    }
out:
    if(f) {
        fclose(f);
    }
    return status;
}

/**
 * Open file
 *
 * @param file      - new descriptor
 * @param filename  - filename
 * @param mode      - standart modes (r,w,b,...)
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_open(wstk_file_t *file, const char *filename, const char *mode) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!file || !filename) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(file->fh) {
        return WSTK_STATUS_ALREADY_OPENED;
    }

    file->fsize = file_get_size(filename);
    if((file->fh = fopen(filename, mode)) == NULL) {
#ifdef WSTK_FILE_DEBUG
        log_error("Couldn't open file");
#endif
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

out:
    if(status != WSTK_STATUS_SUCCESS) {
        if(file->fh) {
            fclose(file->fh);
        }
        file->fh = NULL;
    }
    return status;
}

/**
 * Close file
 *
 * @param file      - file descriptor
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_close(wstk_file_t *file) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!file) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(file->fh) {
        fclose(file->fh);
    }

    file->fh = NULL;

    return WSTK_STATUS_SUCCESS;
}

/**
 * Move file pointer
 *
 * @param file      - the descriptor
 * @param ofs       - offset
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_seek(wstk_file_t *file, size_t ofs) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    int err = 0;

    if(!file || !file->fh) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    err = fseek(file->fh, ofs, SEEK_SET);
    return (err == 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
}

/**
 * Get current position
 *
 * @param file      - the descriptor
 * @param ofs       - the result
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_tell(wstk_file_t *file, size_t *pos) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!file || !file->fh) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *pos = ftell(file->fh);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Read block from file
 *
 * @param file      - the descriptor
 * @param mbuf      - the buffer
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_read(wstk_file_t *file, wstk_mbuf_t *mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *ptr = NULL;
    size_t rc =0;

    if(!file || !file->fh) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    ptr = (char *)wstk_mbuf_buf(mbuf);
    rc = fread(ptr, 1, wstk_mbuf_size(mbuf), file->fh);
    if(rc >= 0) {
        if(rc == 0) {
            status = WSTK_STATUS_NODATA;
        }
        wstk_mbuf_set_pos(mbuf, rc);
        wstk_mbuf_set_end(mbuf, mbuf->pos);
    } else {
        status = WSTK_STATUS_FALSE;
    }

    return status;
}

/**
 * Write block from file
 *
 * @param file      - the descriptor
 * @param mbuf      - the buffer
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_write(wstk_file_t *file, wstk_mbuf_t *mbuf) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *ptr = NULL;
    size_t rc =0;

    if(!file || !file->fh) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    ptr = (char *)wstk_mbuf_buf(mbuf);
    rc = fread(ptr, 1, wstk_mbuf_end(mbuf), file->fh);
    if(rc >= 0) {
        wstk_mbuf_set_pos(mbuf, rc);
    } else {
        status = WSTK_STATUS_FALSE;
    }

    return status;
}

/**
 * Concat filename
 *
 * @param path      - result
 * @param dir       - base dir
 * @param file      - filename
 * @param delimeter - path delimeter
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_name_concat(char **path, const char *dir, const char *file, char delimeter) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *path_local = NULL;
    char *fname_ptr = (char *)file;
    size_t len = 0;
    bool has_slash = false;

    if(!path || !dir || !file || !delimeter) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    len = strlen(dir);
    has_slash = (len >1 && dir[len - 1] == delimeter);

    if(*fname_ptr == delimeter) {
        while(*fname_ptr && *fname_ptr == delimeter) {
            fname_ptr++;
        }
    }
    if(!*fname_ptr) {
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(has_slash) {
        status = wstk_sdprintf(&path_local, "%s%s", dir, fname_ptr);
    } else {
        status = wstk_sdprintf(&path_local, "%s%c%s", dir, delimeter, fname_ptr);
    }

    *path = path_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(path_local);
    }
    return status;
}

/**
 * Extract filename from the path
 *
 * @param fname     - result
 * @param path      - path
 * @param path_len  - legth
 * @param delimeter - path delimiter
 *
 * @return success or some error
 **/
wstk_status_t wstk_file_name_extract(wstk_pl_t *fname, char *path, size_t path_len, char delimeter) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *str_out = NULL;
    char *ptr = NULL, *end = NULL;

    if(!fname || !path || !path_len || !delimeter) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    end = ptr = (path + path_len);
    while(ptr != path) {
        if(*ptr == delimeter) { ptr++; break; }
        ptr--;
    }

    if(ptr == path) {
        return WSTK_STATUS_FALSE;
    }

    fname->p = ptr;
    fname->l = (end - ptr);

    return WSTK_STATUS_SUCCESS;
}

