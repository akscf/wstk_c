/**
 ** based on libre
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_WEBSOCK_H
#define WSTK_WEBSOCK_H
#include <wstk-core.h>
#include <wstk-net.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Data frames */
    WEBSOCK_CONT  = 0x0,
    WEBSOCK_TEXT  = 0x1,
    WEBSOCK_BIN   = 0x2,
    /* Control frames */
    WEBSOCK_CLOSE = 0x8,
    WEBSOCK_PING  = 0x9,
    WEBSOCK_PONG  = 0xa,
} websock_opcode_e;

typedef enum {
    WEBSOCK_NORMAL_CLOSURE   = 1000,
    WEBSOCK_GOING_AWAY       = 1001,
    WEBSOCK_PROTOCOL_ERROR   = 1002,
    WEBSOCK_UNSUPPORTED_DATA = 1003,
    WEBSOCK_INVALID_PAYLOAD  = 1007,
    WEBSOCK_POLICY_VIOLATION = 1008,
    WEBSOCK_MESSAGE_TOO_BIG  = 1009,
    WEBSOCK_EXTENSION_ERROR  = 1010,
    WEBSOCK_INTERNAL_ERROR   = 1011,
} websock_scode_e;

typedef struct {
    unsigned    fin:1;
    unsigned    rsv1:1;
    unsigned    rsv2:1;
    unsigned    rsv3:1;
    unsigned    opcode:4;
    unsigned    mask:1;
    uint64_t    len;
    uint8_t     mkey[4];
} wstk_websock_hdr_t;

wstk_status_t wstk_websock_decode(wstk_websock_hdr_t *hdr, wstk_mbuf_t *mb);
wstk_status_t wstk_websock_encode(wstk_mbuf_t *mb, bool fin, websock_opcode_e opcode, bool mask, size_t len);

wstk_status_t wstk_websock_vsend(wstk_socket_t *sock, websock_opcode_e opcode, websock_scode_e scode, bool server, const char *fmt, va_list ap);

wstk_status_t wstk_websock_s2c_send(wstk_socket_t *sock, websock_opcode_e opcode, const char *fmt, ...);
wstk_status_t wstk_websock_c2s_send(wstk_socket_t *sock, websock_opcode_e opcode, const char *fmt, ...);


#ifdef __cplusplus
}
#endif
#endif
