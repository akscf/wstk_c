/**
 **
 ** (C)2024 aks
 **/
#ifndef WSTK_ATOMIC_H
#define WSTK_ATOMIC_H

#ifdef WSTK_HAVE_ATOMIC
#include "re_atomic.h"

#define wstk_atomic_rlx(_a) re_atomic_rlx(_a)
#define wstk_atomic_rlx_set(_a, _v) re_atomic_rlx_set(_a, _v)
#define wstk_atomic_rlx_add(_a, _v) re_atomic_rlx_add(_a, _v)
#define wstk_atomic_rlx_sub(_a, _v) re_atomic_rlx_sub(_a, _v)

#define wstk_atomic_acq(_a) re_atomic_acq(_a)
#define wstk_atomic_rls_set(_a, _v) re_atomic_rls_set(_a, _v)
#define wstk_atomic_acq_add(_a, _v) re_atomic_acq_add(_a, _v)
#define wstk_atomic_acq_sub(_a, _v) re_atomic_acq_sub(_a, _v)

#define wstk_atomic_seq(_a) re_atomic_seq(_a)
#define wstk_atomic_seq_set(_a, _v) re_atomic_seq_set(_a, _v)
#define wstk_atomic_seq_add(_a, _v) re_atomic_seq_add(_a, _v)
#define wstk_atomic_seq_sub(_a, _v) re_atomic_seq_sub(_a, _v)

#endif


#endif
