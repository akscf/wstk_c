/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-rand.h>
#include <wstk-log.h>
#include <wstk-time.h>

static uint32_t randseed = 0;
static const unsigned char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

wstk_status_t wstk_pvt_rand_init() {

    randseed = wstk_time_epoch_now();
    srand(randseed);

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pvt_rand_shutdown() {

    return WSTK_STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint16_t wstk_rand_u16(void) {
    return (uint16_t)(wstk_rand_u32() >> 16);
}

uint32_t wstk_rand_u32(void) {
    uint32_t v = 0;

    if(randseed == 0) {
        log_error("Seed haven't been initialized!");
    }

#ifdef HAVE_ARC4RANDOM
    v = arc4random();
#else
    v = rand();
#endif
    return v;
}

uint32_t wstk_rand_range(uint32_t min, uint32_t max) {
    return wstk_rand_u32() % ((max + 1) - min) + min;
}

uint64_t wstk_rand_u64(void) {
    return (uint64_t) wstk_rand_u32() << 32 | wstk_rand_u32();
}

char wstk_rand_char(void) {
    char s[2] = { 0 };

    wstk_rand_str(s, sizeof(s));
    return s[0];
}

void wstk_rand_str(char *str, size_t size) {
    size_t i;

    if (!str || !size) {
        return;
    }
    --size;

    wstk_rand_bytes((uint8_t *)str, size);
    for (i=0; i<size; i++) {
        str[i] = alphanum[((uint8_t)str[i]) % (sizeof(alphanum)-1)];
    }

    str[size] = '\0';
}

void wstk_rand_bytes(uint8_t *p, size_t size) {
#ifdef HAVE_ARC4RANDOM
    arc4random_buf(p, size);
#else
    while (size--) {
        p[size] = wstk_rand_u32();
    }
#endif
}
