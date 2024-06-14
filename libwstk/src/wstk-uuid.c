/**
 ** UUID v4
 **
 ** (C)2019 aks
 **/
#include <wstk-uuid.h>
#include <wstk-log.h>
#include <wstk-rand.h>
#include <wstk-regex.h>
#include <wstk-pl.h>
#include <wstk-fmt.h>
#include <wstk-mem.h>
#include <wstk-str.h>

struct wstk_uuid_s {
    uint32_t    o1;
    uint16_t    o2;
    uint16_t    o3;
    uint16_t    o4;
    uint32_t    o5;
    uint16_t    o6;
};

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Get UUID struc tsize
 *
 * @return the size
 **/
size_t wstk_uuid_type_size() {
    return sizeof(wstk_uuid_t);
}

/**
 * Allocate a new UUID
 *
 * @param uuid - a new uuid
 *
 * @return success or error
 **/
wstk_status_t wstk_uuid_alloc(wstk_uuid_t **uuid) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_uuid_t *ul = NULL;

    status = wstk_mem_zalloc((void *)&ul, sizeof(wstk_uuid_t), NULL);
    if(status != WSTK_STATUS_SUCCESS) {
        return status;
    }

    *uuid = ul;
    return WSTK_STATUS_SUCCESS;
}

/**
 * Genrate a new UUID
 *
 * @param uuid - a new uuid
 *
 * @return success or error
 **/
wstk_status_t wstk_uuid_generate(wstk_uuid_t *uuid) {
    uint8_t bb[17] = {0};   // 16 + '0'
    uint8_t *p = (uint8_t *)bb;

    if(!uuid) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wstk_rand_bytes(p, sizeof(bb));
    p[6] = 0x40 | (p[6] & 0x0f);
    p[8] = 0x80 | (p[8] & 0x3f);

    memcpy(&uuid->o1, p, sizeof(uuid->o1));
    p += sizeof(uuid->o1);
    memcpy(&uuid->o2, p, sizeof(uuid->o2));
    p += sizeof(uuid->o2);
    memcpy(&uuid->o3, p, sizeof(uuid->o3));
    p += sizeof(uuid->o3);
    memcpy(&uuid->o4, p, sizeof(uuid->o4));
    p += sizeof(uuid->o4);
    memcpy(&uuid->o5, p, sizeof(uuid->o5));
    p += sizeof(uuid->o5);
    memcpy(&uuid->o6, p, sizeof(uuid->o6));

    return WSTK_STATUS_SUCCESS;
}

/**
 * Convert uuid to a string
 *
 * @param uuid      - the uuid
 * @param str       - output buffer
 * @param str_len   - output buffer length
 *
 * @return success or error
 **/
wstk_status_t wstk_uuid_to_str(wstk_uuid_t *uuid, char *buf, size_t buf_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    int err = 0;

    if(!uuid || !buf || !buf_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    err = wstk_snprintf(buf, buf_len, "%08x-%04x-%04x-%04x-%08x%04x", uuid->o1, uuid->o2, uuid->o3, uuid->o4, uuid->o5, uuid->o6);
    if(err < 0 ) {
        status = WSTK_STATUS_FALSE;
    }

    return status;
}

/**
 * Recovery uuid from a string
 *
 * @param uuid     - a new uuid
 * @param str      - uuid as a string
 * @param str_len  - string length
 *
 * @return success or error
 **/
wstk_status_t wstk_uuid_from_str(wstk_uuid_t *uuid, char *str, size_t str_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    wstk_pl_t pl1,pl2,pl3,pl4,pl5,pl6;

    if(!uuid || !str || !str_len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_regex(str, str_len, "[A-F|a-f|0-9]8\\-[A-F|a-f|0-9]4\\-[A-F|a-f|0-9]4\\-[A-F|a-f|0-9]4\\-[A-F|a-f|0-9]8[A-F|a-f|0-9]4", &pl1, &pl2, &pl3, &pl4, &pl5, &pl6);
    if(status != WSTK_STATUS_SUCCESS) {
        if(status == WSTK_STATUS_NOT_FOUND) { status = WSTK_STATUS_INVALID_VALUE; }
        return status;
    }

    uuid->o1 = wstk_pl_x32(&pl1);
    uuid->o2 = wstk_pl_x32(&pl2);
    uuid->o3 = wstk_pl_x32(&pl3);
    uuid->o4 = wstk_pl_x32(&pl4);
    uuid->o5 = wstk_pl_x32(&pl5);
    uuid->o6 = wstk_pl_x32(&pl6);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Compare two uuids
 *
 * @param u1     - first uuid
 * @param u2     - second uuid
 *
 * @return success or error
 **/
bool wstk_uuid_is_eq(wstk_uuid_t *u1, wstk_uuid_t *u2) {
    if(u1 == u2) {
        return true;
    }
    if(!u1 || !u2) {
        return false;
    }

    if(u1->o1 == u2->o1 && u1->o2 == u2->o2 && u1->o3 == u2->o3 && u1->o4 == u2->o4 && u1->o5 == u2->o5 && u1->o6 == u2->o6) {
        return true;
    }

    return false;
}
