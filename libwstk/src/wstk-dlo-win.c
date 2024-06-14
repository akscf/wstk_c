/**
 **
 ** (C)2024 aks
 **/
#include <wstk-dlo.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>

struct wstk_dlo_s {
    char        *path;
    void        *hnd;
};

static void destructor__wstk_dlo_t(void *data) {
    wstk_dlo_t *dlo = (wstk_dlo_t *)data;

    if(!dlo) { return; }

    if(dlo->hnd) {
        if(!FreeLibrary(dlo->hnd)) {
            log_error("FreeLibrary: failed (dlo=%p, hnd=%p)", dlo, dlo->hnd);
        }
    }

    wstk_mem_deref(dlo->path);
#ifdef WSTK_DLO_DEBUG
    WSTK_DBG_PRINT("dlo destroyed: %p", dlo);
#endif

}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_dlo_open(wstk_dlo_t **dlo, char *path) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_dlo_t *dlo_local = NULL;
    HINSTANCE os_handle;

    if(!path || !dlo) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&dlo_local, sizeof(wstk_dlo_t), destructor__wstk_dlo_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    dlo_local->path = (char *)wstk_str_dup(path);

    os_handle = LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!os_handle) {
        os_handle = LoadLibraryA(path);
    }

    if(!os_handle) {
        log_error("LoadLibrary: failed (path=%s)", path);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    dlo_local->hnd = (void *)os_handle ;

    *dlo = dlo_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(dlo_local);
    }
    return status;
}

wstk_status_t wstk_dlo_close(wstk_dlo_t **dlo) {
    wstk_dlo_t *dlo_local = *dlo;

    if(!dlo || !dlo_local) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *dlo = wstk_mem_deref(dlo_local);
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_dlo_sym(wstk_dlo_t *dlo, const char *sym_name, void **sym) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!dlo || !sym_name || !sym) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *sym = (void *)GetProcAddress(dlo->hnd, sym_name);
    if(!*sym) {
        log_warn("GetProcAddress: not found (%s)", sym_name);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

out:
    return status;
}

