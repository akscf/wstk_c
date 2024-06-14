/**
 *
 * (C)2019 aks
 **/
#ifndef WSTK_UUID_H
#define WSTK_UUID_H
#include <wstk-core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_UUID_STRING_SIZE 36

typedef struct wstk_uuid_s wstk_uuid_t;

wstk_status_t wstk_uuid_alloc(wstk_uuid_t **uuid);
size_t wstk_uuid_type_size();

wstk_status_t wstk_uuid_generate(wstk_uuid_t *uuid);

wstk_status_t wstk_uuid_to_str(wstk_uuid_t *uuid, char *buf, size_t buf_len);
wstk_status_t wstk_uuid_from_str(wstk_uuid_t *uuid, char *str, size_t str_len);

bool wstk_uuid_is_eq(wstk_uuid_t *u1, wstk_uuid_t *u2);


#ifdef __cplusplus
}
#endif
#endif
