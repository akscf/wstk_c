/**
 * (C)2024 aks
 **/
#ifndef WSTK_TYPES_H
#define WSTK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef true
 #define true 1
#endif
#ifndef false
 #define false 0
#endif


#ifndef MIN
 #define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif
#ifndef MAX
 #define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#define ARRAY_SIZE(a) ((sizeof(a))/(sizeof((a)[0])))

typedef intptr_t    wstk_intptr_t;


/**
 * https://www.gnu.org/software/libc/manual/html_node/Infinity-and-NaN.html
 **/
#ifdef WSTK_BUILTIN_FPC
 #undef     NAN
 #define    NAN (0.0/0.0)
 #undef     INFINITY
 #define    INFINITY 1e9999
#endif


typedef struct wstk_mbuf_s {
    uint8_t     *buf;
    size_t      size;
    size_t      pos;
    size_t      end;
} wstk_mbuf_t;

typedef struct wstk_pl_s {
    const char  *p;
    size_t      l;
} wstk_pl_t;



#ifdef __cplusplus
}
#endif
#endif
