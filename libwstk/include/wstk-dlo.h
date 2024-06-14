/**
 ** Dynamic Loadable Module
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_DLO_H
#define WSTK_DLO_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wstk_dlo_s wstk_dlo_t;

/**
 * Open and load module
 *
 * @param dlo   - module destriptor
 * @param path  - module name or a full qualified path
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_dlo_open(wstk_dlo_t **dlo, char *path);

/**
 * Close module and destroy descriptor
 *
 * @param dlo - module destriptor
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_dlo_close(wstk_dlo_t **dlo);

/**
 * Lookup a symbol in the module
 *
 * @param dlo       - module destriptor
 * @param sym_name  - ascii name
 * @param sym       - pinter to the symbol
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_dlo_sym(wstk_dlo_t *dlo, const char *sym_name, void **sym);



#ifdef __cplusplus
}
#endif
#endif
