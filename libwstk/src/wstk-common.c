/**
 **
 ** (C)2024 aks
 **/
#include <wstk.h>
#include <wstk-log.h>
#include <wstk-fmt.h>

void wstk_dbg_hexdump(const void *p, size_t len) {
    const uint8_t *buf = p;
    uint32_t j;
    size_t i;

    if(!buf) return;

    for (i=0; i < len; i += 16) {
        printf("%08x ", (uint32_t) i);
        for (j=0; j<16; j++) {
            const size_t pos = i+j;
            if (pos < len) printf(" %02x", buf[pos]);
            else printf("   ");
            if (j == 7) printf("  ");
        }
        printf("  |");
        for (j=0; j<16; j++) {
            const size_t pos = i+j;
            uint8_t v;
            if (pos >= len) break;
            v = buf[pos];
            printf("%c", isprint(v) ? v : '.');
            if (j == 7) printf(" ");
        }
        printf("|\n");
    }
}

