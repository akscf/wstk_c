/**
 **
 ** (C)2019 aks
 **/
#include <wstk-dir.h>
#include <wstk-log.h>
#include <wstk-file.h>
#include <wstk-mem.h>
#include <wstk-fmt.h>
#include <wstk-str.h>

#ifdef WSTK_OS_WIN
 #define os_mkdir(d, f) mkdir(d)
 extern wstk_status_t winos_dir_list(const char *base, char separator, wstk_dir_list_handler_t handler, void *udata);
#else
 #define os_mkdir(d, f) mkdir(d, f)
#endif

static wstk_status_t dir_create_recursive(const char *dirname) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char tmp[WSTK_MAXNAMLEN];
    char *p = NULL;
    size_t len;
    int err;

    wstk_snprintf(tmp, sizeof(tmp), "%s", dirname);
    len = wstk_str_len((char *)tmp);

    if(tmp[len - 1] == WSTK_PATH_DELIMITER) {
        tmp[len - 1] = 0;
    }

    for(p = tmp + 1; *p; p++) {
        if(*p == WSTK_PATH_DELIMITER) {
            *p = 0;
            err = os_mkdir(tmp, S_IRWXU);
            if(err && errno != EEXIST) {
                status = (errno == 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
                break;
            }
            *p = WSTK_PATH_DELIMITER;
        }
    }

    if(status == WSTK_STATUS_SUCCESS) {
        err = os_mkdir(tmp, S_IRWXU);
        if(err && errno != EEXIST) {
            status = (errno == 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);
        }
    }

    return status;
}

static bool is_dir(char *path) {
    struct stat st = { 0 };

    if(stat(path, &st) == -1) {
        return false;
    }

    return S_ISDIR(st.st_mode);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Check dir on exist
 *
 * @param dirname - dir name
 *
 * @return true/false
 **/
bool wstk_dir_exists(const char *dirname) {
    struct stat st = {0};

    if(!dirname) {
        return false;
    }
    if(stat(dirname, &st) == -1) {
        return false;
    }

    return(st.st_mode & S_IFDIR ? true : false);
}

/**
 * Create dir
 *
 * @param dirname       - dir name
 * @param recursive     - true if want to crete all the subdirs
 *
 * @return true/false
 **/
wstk_status_t wstk_dir_create(const char *dirname, bool recursive) {
    if(!dirname) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!recursive) {
        if(os_mkdir(dirname, S_IRWXU) == 0) {
            return WSTK_STATUS_SUCCESS;
        }
        if(errno == EEXIST) {
            return WSTK_STATUS_SUCCESS;
        }
        return errno;
    }

    return dir_create_recursive(dirname);
}

/**
 * Browse directory
 *
 * @param base          - base dir
 * @param delimiter     - path delimiter
 * @param handler       - will be called for each item
 * @param udata         - custom user data
 *
 * @return true/false
 **/
wstk_status_t wstk_dir_list(const char *base, char delimiter, wstk_dir_list_handler_t handler, void *udata) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    DIR *dir = NULL;
    char *name_buf = NULL;
    size_t name_buf_len = (WSTK_MAXNAMLEN * 2);
    struct dirent *de = NULL;
    wstk_dir_entry_t wstk_de = { 0 };
    wstk_file_meta_t file_meta = { 0 };
    bool has_end_slash = false;
    size_t slen = 0;

    if(!base || !handler || !delimiter) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if(!(dir = opendir(base))) {
        if(errno == ENOENT) {
            wstk_goto_status(WSTK_STATUS_NOT_FOUND, out);
        }
        log_error("Couldn't open dir (err=%d)", errno);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((status = wstk_mem_alloc((void *)&name_buf, name_buf_len, NULL)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    slen = strlen(base);
    has_end_slash = (base[slen - 1] == delimiter);

    while((de = readdir(dir)) != NULL) {
#ifdef _DIRENT_HAVE_D_NAMLEN
        slen = de->d_namlen;
#else
        slen = wstk_str_len((char *) de->d_name);
#endif
        if((slen == 1 && de->d_name[0] == '.') || (slen == 2 && de->d_name[0] == '.' && de->d_name[1] == '.')) {
            continue;
        }

        wstk_de.name = (char *)de->d_name;

        if(has_end_slash) {
            wstk_snprintf(name_buf, name_buf_len, "%s%s", base, wstk_de.name);
        } else {
            wstk_snprintf(name_buf, name_buf_len, "%s%c%s", base, delimiter, wstk_de.name);
        }

#if defined(WSTK_OS_WIN) || defined(WSTK_OS_SUNOS)
        wstk_de.directory = is_dir(name_buf);
#elif defined(WSTK_OS_OS2)
        wstk_de.directory = (de->d_attr & A_DIR ? true : false);
#else
        wstk_de.directory = (de->d_type & DT_DIR ? true : false);
#endif

        wstk_de.size = 0;
        wstk_de.mtime = 0;
        wstk_de.path = name_buf;

        status = wstk_file_get_meta(wstk_de.path, &file_meta);
        if(status == WSTK_STATUS_SUCCESS) {
            wstk_de.mtime = file_meta.mtime;
            wstk_de.size = (wstk_de.directory ? 0 : file_meta.size);
        }

        if(handler((wstk_dir_entry_t *)&wstk_de, udata) != WSTK_STATUS_SUCCESS) {
            break;
        }
    }

out:
    wstk_mem_deref(name_buf);
    if(dir) {
        closedir(dir);
    }
    return status;
}
