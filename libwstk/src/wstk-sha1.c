/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-sha1.h>
#include <wstk-log.h>
#include <wstk-str.h>

#define SHA1HANDSOFF 1

// ----------------------------------------------------------------------------------------------------------
#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#if defined (BYTE_ORDER) && defined(BIG_ENDIAN) && (BYTE_ORDER == BIG_ENDIAN)
#define WORDS_BIGENDIAN 1
#endif
#ifdef _BIG_ENDIAN
#define WORDS_BIGENDIAN 1
#endif


/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
/* FIXME: can we do this in an endian-proof way? */
#ifdef WORDS_BIGENDIAN
#define blk0(i) block->l[i]
#else
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xff00ff00) |(rol(block->l[i],8)&0x00ff00ff))
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) \
        z+=((w&(x^y))^y)+blk0(i)+0x5a827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) \
        z+=((w&(x^y))^y)+blk(i)+0x5a827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) \
        z+=(w^x^y)+blk(i)+0x6ed9eba1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) \
        z+=(((w|x)&y)|(w&x))+blk(i)+0x8f1bbcdc+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) \
        z+=(w^x^y)+blk(i)+0xca62c1d6+rol(v,5);w=rol(w,30);

// ----------------------------------------------------------------------------------------------------------
/* Hash a single 512-bit block. This is the core of the algorithm. */
static void SHA1_Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    typedef union {
        uint8_t c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16* block;

#ifdef SHA1HANDSOFF
    CHAR64LONG16 workspace;
    block = &workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif

    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0);
    R0(e,a,b,c,d, 1);
    R0(d,e,a,b,c, 2);
    R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4);
    R0(a,b,c,d,e, 5);
    R0(e,a,b,c,d, 6);
    R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8);
    R0(b,c,d,e,a, 9);
    R0(a,b,c,d,e,10);
    R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12);
    R0(c,d,e,a,b,13);
    R0(b,c,d,e,a,14);
    R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16);
    R1(d,e,a,b,c,17);
    R1(c,d,e,a,b,18);
    R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20);
    R2(e,a,b,c,d,21);
    R2(d,e,a,b,c,22);
    R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24);
    R2(a,b,c,d,e,25);
    R2(e,a,b,c,d,26);
    R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28);
    R2(b,c,d,e,a,29);
    R2(a,b,c,d,e,30);
    R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32);
    R2(c,d,e,a,b,33);
    R2(b,c,d,e,a,34);
    R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36);
    R2(d,e,a,b,c,37);
    R2(c,d,e,a,b,38);
    R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40);
    R3(e,a,b,c,d,41);
    R3(d,e,a,b,c,42);
    R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44);
    R3(a,b,c,d,e,45);
    R3(e,a,b,c,d,46);
    R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48);
    R3(b,c,d,e,a,49);
    R3(a,b,c,d,e,50);
    R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52);
    R3(c,d,e,a,b,53);
    R3(b,c,d,e,a,54);
    R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56);
    R3(d,e,a,b,c,57);
    R3(c,d,e,a,b,58);
    R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60);
    R4(e,a,b,c,d,61);
    R4(d,e,a,b,c,62);
    R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64);
    R4(a,b,c,d,e,65);
    R4(e,a,b,c,d,66);
    R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68);
    R4(b,c,d,e,a,69);
    R4(a,b,c,d,e,70);
    R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72);
    R4(c,d,e,a,b,73);
    R4(b,c,d,e,a,74);
    R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76);
    R4(d,e,a,b,c,77);
    R4(c,d,e,a,b,78);
    R4(b,c,d,e,a,79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;

    /* Wipe variables */
    a = b = c = d = e = 0;
}

/**
 * Initialize new context
 *
 * @param context SHA1-Context
 */
static void SHA1_Init(SHA1_CTX* context) {
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
    context->state[4] = 0xc3d2e1f0;
    context->count[0] = context->count[1] = 0;
}


/**
 * Run your data through this
 *
 * @param context SHA1-Context
 * @param p       Buffer to run SHA1 on
 * @param len     Number of bytes
 */
static void SHA1_Update(SHA1_CTX* context, const void *p, size_t len) {
    const uint8_t* data = p;
    size_t i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += (uint32_t)(len << 3)) < (len << 3)) {
        context->count[1]++;
    }
    context->count[1] += (uint32_t)(len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        SHA1_Transform(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            SHA1_Transform(context->state, data + i);
        }
        j = 0;
    } else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}


/**
 * Add padding and return the message digest
 *
 * @param digest  Generated message digest
 * @param context SHA1-Context
 */
static void SHA1_Final(uint8_t digest[WSTK_SHA1_DIGEST_SIZE], SHA1_CTX* context) {
    uint32_t i;
    uint8_t  finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 255);
    }
    SHA1_Update(context, (uint8_t *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        SHA1_Update(context, (uint8_t *)"\0", 1);
    }
    SHA1_Update(context, finalcount, 8); /* Should cause SHA1_Transform */
    for (i = 0; i < WSTK_SHA1_DIGEST_SIZE; i++) {
        digest[i] = (uint8_t) ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }

    /* Wipe variables */
    i = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(finalcount, 0, 8);	/* SWR */

#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite its own static vars */
    SHA1_Transform(context->state, context->buffer);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Initial SHA1 ctx
 *
 * @param ctx  - the context
 *
 * @return success or error
 **/
wstk_status_t wstk_sha1_init(wstk_sha1_t *ctx) {
    if(!ctx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    SHA1_Init(ctx);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Update SHA1 context
 *
 * @param ctx   - the context
 * @param data  - some data
 * @param len   - data length
 *
 * @return success or error
 **/
wstk_status_t wstk_sha1_update(wstk_sha1_t *ctx, const uint8_t *data, size_t len) {
    if(!ctx || !data || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    SHA1_Update(ctx, data, len);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Finalize and generate digest
 *
 * @param ctx       - the context
 * @param digest    - digest buffer (WSTK_SHA1_DIGEST_SIZE)
 *
 * @return success or error
 **/
wstk_status_t wstk_sha1_final(wstk_sha1_t *ctx, uint8_t *digest) {
    if(!ctx || !digest) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    SHA1_Final(digest, ctx);
    return WSTK_STATUS_SUCCESS;
}

/**
 * Simple digest
 *
 * @param digest    - digest buffer (WSTK_SHA1_DIGEST_SIZE)
 * @param data      - some data
 * @param len       - data length
 *
 * @return success or error
 **/
wstk_status_t wstk_sha1_digest(uint8_t *digest, const uint8_t *data, size_t len) {
    wstk_sha1_t ctx;

    if(!digest || !data || !len) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    SHA1_Final(digest, &ctx);

    return WSTK_STATUS_SUCCESS;
}

/**
 * Digest to hex
 *
 * @param hex       - buffer (WSTK_SHA1_DIGEST_STRING_SIZE)
 * @param digest    - binary digest
 *
 * @return success or error
 **/
wstk_status_t wstk_sha1_digest2hex(uint8_t *hex, uint8_t *digest) {
    if(!hex || !digest) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    return wstk_str_bin2hex((char *)hex, digest, WSTK_SHA1_DIGEST_SIZE);
}

/**
 * Compare two digest
 *
 * @param dig1  - digest 1
 * @param dig2  - digest 2
 *
 * @return true is equal
 **/
bool wstk_sha1_is_equal(uint8_t *dig1, uint8_t *dig2) {
    if(dig1 == dig2) {
        return true;
    }
    if(!dig1 || !dig2) {
        return false;
    }

    if(memcmp(dig1, dig2, WSTK_SHA1_DIGEST_SIZE) == 0) {
        return true;
    }
    return false;
}
