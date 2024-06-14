/**
 ** based on libre
 **
 ** (C)2024 aks
 **/
#include <wstk-websock.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mbuf.h>
#include <wstk-net.h>
#include <wstk-rand.h>
#include <wstk-endian.h>


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Decode websock header
 *
 * @param hdr
 * @param mb
 *
 * @return success or error
 **/
wstk_status_t wstk_websock_decode(wstk_websock_hdr_t *hdr, wstk_mbuf_t *mb) {
    uint8_t v, *p;
    size_t i;

    if(wstk_mbuf_left(mb) < 2) {
        return WSTK_STATUS_NODATA;
    }

    wstk_mbuf_read_u8(mb, &v);
    hdr->fin    = v>>7 & 0x1;
    hdr->rsv1   = v>>6 & 0x1;
    hdr->rsv2   = v>>5 & 0x1;
    hdr->rsv3   = v>>4 & 0x1;
    hdr->opcode = v    & 0x0f;

    wstk_mbuf_read_u8(mb, &v);
    hdr->mask = v>>7 & 0x1;
    hdr->len  = v    & 0x7f;

    if(hdr->len == 126) {
        uint16_t u16 = 0;

        if(wstk_mbuf_left(mb) < 2) {
            return WSTK_STATUS_NODATA;
        }

        wstk_mbuf_read_u16(mb, &u16);
        hdr->len = ntohs(u16);

    } else if(hdr->len == 127) {
        uint64_t u64 = 0;

        if(wstk_mbuf_left(mb) < 8) {
            return WSTK_STATUS_NODATA;
        }

        wstk_mbuf_read_u64(mb, &u64);
        hdr->len = wstk_ntohll(u64);
    }

    if(hdr->mask) {
        if(wstk_mbuf_left(mb) < (4 + hdr->len)) {
            return WSTK_STATUS_NODATA;
        }

        wstk_mbuf_read_u8(mb, &hdr->mkey[0]);
        wstk_mbuf_read_u8(mb, &hdr->mkey[1]);
        wstk_mbuf_read_u8(mb, &hdr->mkey[2]);
        wstk_mbuf_read_u8(mb, &hdr->mkey[3]);

        for (i=0, p=wstk_mbuf_buf(mb); i<hdr->len; i++) {
            p[i] = p[i] ^ hdr->mkey[i%4];
        }
    } else {
        if(wstk_mbuf_left(mb) < hdr->len) {
            return WSTK_STATUS_NODATA;
        }
    }

    return WSTK_STATUS_SUCCESS;
}


/**
 * Encode websock header
 *
 * @param mb
 * @param fin
 * @param opcode
 * @param mask
 * @param len
 *
 * @return success or error
 **/
wstk_status_t wstk_websock_encode(wstk_mbuf_t *mb, bool fin, websock_opcode_e opcode, bool mask, size_t len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if((status = wstk_mbuf_write_u8(mb, (fin<<7) | (opcode & 0x0f))) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if(len > 0xffff) {
        if((status = wstk_mbuf_write_u8(mb, (mask<<7) | 127)) != WSTK_STATUS_SUCCESS) {
            goto out;
        }
        if((status = wstk_mbuf_write_u64(mb, wstk_htonll(len))) != WSTK_STATUS_SUCCESS) {
           goto out;
        }
    } else if (len > 125) {
        if((status = wstk_mbuf_write_u8(mb, (mask<<7) | 126)) != WSTK_STATUS_SUCCESS) {
            goto out;
        }
        if((status = wstk_mbuf_write_u16(mb, htons(len))) != WSTK_STATUS_SUCCESS)  {
            goto out;
        }
    } else {
        if((status = wstk_mbuf_write_u8(mb, (mask<<7) | len)) != WSTK_STATUS_SUCCESS) {
            goto out;
        }
    }

    if(mask) {
        uint8_t mkey[4];
        uint8_t *p;
        size_t i;

        wstk_rand_bytes(mkey, sizeof(mkey));

        if((status = wstk_mbuf_write_mem(mb, mkey, sizeof(mkey))) != WSTK_STATUS_SUCCESS) {
            goto out;
        }

        for (i=0, p=wstk_mbuf_buf(mb); i<len; i++) {
            p[i] = p[i] ^ mkey[i%4];
        }
    }

out:
    return status;
}

/**
 *
 *
 **/
wstk_status_t wstk_websock_vsend(wstk_socket_t *sock, websock_opcode_e opcode, websock_scode_e scode, bool server, const char *fmt, va_list ap) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    const size_t hsz = (server ? 10 : 14);
    wstk_mbuf_t *mb = NULL;
    size_t len, start;
    int err = 0;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    if((status = wstk_mbuf_alloc(&mb, 2048)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    mb->pos = hsz;
    if(scode) {
        if((status = wstk_mbuf_write_u16(mb, htons(scode))) != WSTK_STATUS_SUCCESS) {
            goto out;
        }
    }
    if(fmt) {
        if((status = wstk_mbuf_vprintf(mb, fmt, ap)) != WSTK_STATUS_SUCCESS) {
            goto out;
        }
    }

    len = (mb->pos - hsz);

    if(len > 0xffff)   { start = mb->pos = 0; }
    else if(len > 125) { start = mb->pos = 6; }
    else { start = mb->pos = 8; }

    if((status = wstk_websock_encode(mb, true, opcode, !server, len)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    mb->pos = start;
    status = wstk_tcp_write(sock, mb, 0);

out:
    wstk_mem_deref(mb);
    return status;
}

/**
 * Server to client
 *
 **/
wstk_status_t wstk_websock_s2c_send(wstk_socket_t *sock, websock_opcode_e opcode, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    va_list ap;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    va_start(ap, fmt);
    status = wstk_websock_vsend(sock, opcode, 0, true, fmt, ap);
    va_end(ap);

    return status;
}

/**
 * Client to server
 *
 **/
wstk_status_t wstk_websock_c2s_send(wstk_socket_t *sock, websock_opcode_e opcode, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    va_list ap;

    if(!sock) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    va_start(ap, fmt);
    status = wstk_websock_vsend(sock, opcode, 0, false, fmt, ap);
    va_end(ap);

    return status;
}
