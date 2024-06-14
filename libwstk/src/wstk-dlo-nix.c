/**
 **
 ** (C)2019 aks
 **/
#include <wstk-dlo.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>

static const int dl_flag = RTLD_NOW | RTLD_LOCAL;

struct wstk_dlo_s {
    char    *path;
    void    *hnd;
};

static void *dlo_open(const char *name) {
    void *hnd = NULL;

    if(!name) {
        return NULL;
    }

    hnd = dlopen(name, dl_flag);
    if(!hnd) {
        log_error("dopen: %s (%s)", name, dlerror());
        return NULL;
    }
    return hnd;
}

static void *dlo_sym(void *hnd, const char *symbol) {
    const char *err;
    void *sym = NULL;

    if(!hnd || !symbol) {
        return NULL;
    }

    (void)dlerror(); // Clear any existing error

    sym = dlsym(hnd, symbol);
    err = dlerror();
    if(err) {
        log_warn("dlsym: %s", err);
        return NULL;
    }
    return sym;
}

static void dlo_close(void *hnd) {
    int err;

    if(!hnd) {
        return;
    }

    err = dlclose(hnd);
    if(err != 0) {
        log_error("dlclose: %d", err);
    }
}

static void destructor__wstk_dlo_t(void *data) {
    wstk_dlo_t *dlo = (wstk_dlo_t *)data;

    if(!dlo) { return; }

    if(dlo->hnd) {
        dlo_close(dlo->hnd);
    }

    wstk_mem_deref(dlo->path);

#ifdef WSTK_DLO_DEBUG
    WSTK_DBG_PRINT("DLO destroyed: %p", dlo);
#endif

}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t wstk_dlo_open(wstk_dlo_t **dlo, char *path) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_dlo_t *dlo_local = NULL;

    if(!path || !dlo) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&dlo_local, sizeof(wstk_dlo_t), destructor__wstk_dlo_t);
    if(status != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    dlo_local->path = wstk_str_dup(path);

    if(!(dlo_local->hnd = dlo_open(dlo_local->path))) {
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

#ifdef WSTK_DLO_DEBUG
    WSTK_DBG_PRINT("DLO created: dlo=%p (%s)", dlo_local, dlo_local->path);
#endif

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

    if(!dlo || !sym_name || !sym) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    *sym = dlo_sym(dlo->hnd, sym_name);
    if(!*sym) {
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}
