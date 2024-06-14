/**
 **
 ** (C)2019 aks
 **/
#include <wstk-dlo.h>
#include <wstk-log.h>
#include <wstk-mem.h>

struct wstk_dlo_s {
    char        *path;
    HMODULE     hnd;
};

static void destructor__wstk_dlo_t(void *data) {
    wstk_dlo_t *dlo = (wstk_dlo_t *)data;
    int rc;

    if(!dlo) { return; }

    if(dlo->hnd) {
        rc = DosFreeModule(dlo->hnd);
        if(rc) {
            log_error("DosFreeModule: failed (err=%d, dlo=%p)", rc, dlo);
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
    char failed_module[200];
    int rc;

    if(!path || !dlo) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&dlo_local, sizeof(wstk_dlo_t), destructor__wstk_dlo_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }
    dlo_local->path = wstk_str_dup(path);

    if((rc = DosLoadModule(failed_module, sizeof(failed_module), dlo_local->path, &dlo_local->hnd)) != 0) {
        log_error("DosLoadModule: err=%d (%s), (path=%s)", rc, (char *)failed_module, path);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

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
    PFN func;
    int rc;

    if(!dlo || !sym_name || !sym) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((rc = DosQueryProcAddr(dlo->hnd, 0, sym_name, &func)) != 0) {
#ifdef WSTK_DLO_DEBUG
        log_warn("DosQueryProcAddr: (err=%d, sym_name=%s)", rc, sym_name);
#endif
        return WSTK_STATUS_FALSE;
    }

    *sym = (void *)func;

    return WSTK_STATUS_FALSE;
}

