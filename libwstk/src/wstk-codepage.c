/**
 **
 ** (C)2024 aks
 **/
#include <wstk-codepage.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-pl.h>
#include <wstk-regex.h>
#include <wstk-mbuf.h>

/*
 * https://en.wikipedia.org/wiki/Windows_code_page
 */
static wstk_codepage_entry_t CODEPAGES[] = {
    { 866,   "CP866"        },
    { 20866, "KOI8-R"       },
    { 1251,  "WINDOWS-1251" },
    { 65001, "UTF-8"        }
};

// -------------------------------------------------------------------------
static uint32_t _system_cp_id = 0;
static const char *_system_cp_name = NULL;

static wstk_codepage_entry_t *cpt_lookup_by_id(uint32_t cpid);
static wstk_codepage_entry_t *cpt_lookup_by_name(wstk_pl_t *name);

wstk_status_t wstk_pvt_codepage_init() {
    wstk_codepage_entry_t *entry = NULL;

    _system_cp_id = wstk_get_system_cp_now();
    if(_system_cp_id > 0) {
        entry = cpt_lookup_by_id(_system_cp_id);
        if(entry) { _system_cp_name = entry->name; }
    }

    return WSTK_STATUS_SUCCESS;
}

static wstk_codepage_entry_t *cpt_lookup_by_id(uint32_t cpid) {
    wstk_codepage_entry_t *entry = NULL;
    bool found = false;
    int i = 0;

    if(!cpid) {
        return NULL;
    }

    for(i = 0; i < ARRAY_SIZE(CODEPAGES); i++) {
        entry = &CODEPAGES[i];
        if(cpid == entry->id) { found = true; break; }
    }

    return(found ? entry : NULL);
}

static wstk_codepage_entry_t *cpt_lookup_by_name(wstk_pl_t *name) {
    wstk_codepage_entry_t *entry = NULL;
    bool found = false;
    int i = 0;

    if(!name) {
        return NULL;
    }

    for(i = 0; i < ARRAY_SIZE(CODEPAGES); i++) {
        entry = &CODEPAGES[i];
        if(wstk_pl_strcasecmp(name, entry->name) == 0) {
            found = true; break;
        }
    }

    return(found ? entry : NULL);
}

static uint32_t unix_detect_codepage(const char *str) {
    wstk_pl_t cs = {0};
    size_t len = wstk_str_len(str);

    if(!str || !len) {
        return 0;
    }

    if(wstk_regex(str, len, ".[^]+", &cs) == WSTK_STATUS_SUCCESS) {
        wstk_codepage_entry_t *cpe = cpt_lookup_by_name(&cs);
        if(cpe) { return cpe->id; }
    }

    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Get system codepage
 *
 * @return codepage or 0
 **/
uint32_t wstk_system_codepage_id() {
    return _system_cp_id;
}

/**
 * Get system codepage
 *
 * @return name or null
 **/
const char *wstk_system_codepage_name() {
    return _system_cp_name;
}


/**
 * Get system codepage
 *
 * @return id or 0
 **/
uint32_t wstk_get_system_cp_now() {
    uint32_t id = 0;

#if defined(WSTK_OS_WIN)
    id = GetACP();
#elif defined(WSTK_OS_OS2)
    unsigned long cpList[16] = {0};
    unsigned long cpSize = 0;
    int rc = 0;

    rc = DosQueryCp(sizeof(cpList), cpList, &cpSize);
    if(rc) {
        log_error("DosQueryCp: err=%d", rc);
        return 0;
    }
    id = cpList[0];
#else
    id = unix_detect_codepage(getenv("LC_ALL"));
    if(!id) { id = unix_detect_codepage(getenv("LC_CTYPE")); }
    if(!id) { id = unix_detect_codepage(getenv("LANG")); }
#endif

    return id;
}

