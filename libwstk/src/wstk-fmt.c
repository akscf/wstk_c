/**
 ** base on libre
 **
 ** (C)2024 aks
 **/
#include <wstk-fmt.h>
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-str.h>
#include <wstk-pl.h>
#include <wstk-net.h>

#ifdef WSTK_OS_WIN
 #include <float.h>
 #ifndef isinf
 #define isinf(d) (!_finite(d))
 #endif
 #ifndef isnan
 #define isnan(d) _isnan(d)
 #endif
#endif

#if WSTK_OS_OS2
 #ifndef isinf
 #define isinf(d) (isnan((d - d)) && !isnan(d))
 #endif
#endif

#ifdef WSTK_OS_SUNOS
 #include <ieeefp.h>
 #undef isinf
 #define isinf(a) (fpclass((a)) == FP_NINF || fpclass((a)) == FP_PINF)
 #undef isnan
 #define isnan(a) isnand((a))
#endif


enum length_modifier {
	LENMOD_NONE      = 0,
	LENMOD_LONG      = 1,
	LENMOD_LONG_LONG = 2,
	LENMOD_SIZE      = 42,
};

enum {
	DEC_SIZE = 42,
	NUM_SIZE = 64
};

static const char prfx_neg[]  = "-";
static const char prfx_hex[]  = "0x";
static const char str_nil[]  = "(nil)";


static int write_padded(const char *p, size_t sz, size_t pad, char pch, bool plr, const char *prfx, wstk_vprintf_h *vph, void *arg) {
	const size_t prfx_len = wstk_str_len(prfx);
	int err = 0;

	pad -= MIN(pad, prfx_len);

	if(prfx && pch == '0') {
        err |= vph(prfx, prfx_len, arg);
	}

	while(!plr && (pad-- > sz)) {
		err |= vph(&pch, 1, arg);
	}

	if(prfx && pch != '0') {
		err |= vph(prfx, prfx_len, arg);
	}

	if(p && sz) {
		err |= vph(p, sz, arg);
	}

	while(plr && pad-- > sz) {
        err |= vph(&pch, 1, arg);
	}

	return err;
}

static uint32_t local_itoa(char *buf, uint64_t n, uint8_t base, bool uc) {
	char c, *p = buf + NUM_SIZE;
	uint32_t len = 1;
	const char a = uc ? 'A' : 'a';

	*--p = '\0';
	do {
		const uint64_t dv  = n / base;
		const uint64_t mul = dv * base;

		c = (char)(n - mul);

		if (c < 10)
			*--p = '0' + c;
		else
			*--p = a + (c - 10);

		n = dv;
		++len;

	} while (n != 0);

	memmove(buf, p, len);

	return len - 1;
}

static size_t local_ftoa(char *buf, double n, size_t dp) {
	char *p = buf;
	long long a = (long long)n;
	double b = n - (double)a;

	b = (b < 0) ? -b : b;

	/* integral part */
	p += local_itoa(p, (a < 0) ? -a : a, 10, false);

	*p++ = '.';

	/* decimal digits */
	while (dp--) {
		char v;

		b *= 10;
		v  = (char)b;
		b -= v;

		*p++ = '0' + (char)v;
	}

	*p = '\0';

	return p - buf;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Print a formatted string
 *
 * @param fmt Formatted string
 * @param ap  Variable argument
 * @param vph Print handler
 * @param arg Handler argument
 *
 * @return 0 if success, otherwise errorcode
 *
 * Extensions:
 *
 * <pre>
 *   %b  (char *, size_t)           Buffer string with pointer and length
 *   %r  (pl_t)                     Pointer-length object
 *   %w  (uint8_t *, size_t)        Binary buffer to hexadecimal format
 *   %H  (wstk_printf_h *, void *)  Print handler with argument
 *   %v  (char *fmt, va_list *)     Variable argument list
 *   %j  (wstk_sockaddr_t *)        Socket address - address part only
 *   %J  (wstk_sockaddr_t*)         Socket address and port - like 1.2.3.4:1234
 * </pre>
 *
 */
int wstk_vhprintf(const char *fmt, va_list ap, wstk_vprintf_h *vph, void *arg) {
	uint8_t base, *bptr;
	char pch, ch, num[NUM_SIZE], addr[64], msg[256];
	enum length_modifier lenmod = LENMOD_NONE;
	wstk_printf_t pf;
	bool fm = false, plr = false;
	const wstk_pl_t *pl;
	size_t pad = 0, fpad = -1, len, i;
	const char *str, *p = fmt, *p0 = fmt;
    const wstk_sockaddr_t *sa;
	wstk_printf_h *ph;
	void *ph_arg;
	va_list *apl;
	int err = 0, sa_af=0;
	uint16_t sa_port = 0;
	void *ptr;
	uint64_t n;
	int64_t sn;
	bool uc = false;
	double dbl;

	if(!fmt || !vph) {
        return WSTK_STATUS_INVALID_PARAM;
	}

	pf.vph = vph;
	pf.arg = arg;

	for(;*p && !err; p++) {
		if(!fm) {
			if (*p != '%') { continue; }

			pch = ' ';
			plr = false;
			pad = 0;
			fpad = -1;
			lenmod = LENMOD_NONE;
			uc = false;

			if (p > p0) {
				err |= vph(p0, p - p0, arg);
			}

			fm = true;
			continue;
		}

		fm = false;
		base = 10;

		switch (*p) {

		case '-':
			plr = true;
			fm  = true;
			break;

		case '.':
			fpad = pad;
			pad = 0;
			fm = true;
			break;

		case '%':
			ch = '%';
			err |= vph(&ch, 1, arg);
			break;

		case 'b':
			str = va_arg(ap, const char *);
			len = va_arg(ap, size_t);
			err |= write_padded(str, str ? len : 0, pad, ' ', plr, NULL, vph, arg);
			break;

		case 'c':
			ch = va_arg(ap, int);
			err |= write_padded(&ch, 1, pad, ' ', plr, NULL, vph, arg);
			break;

		case 'd':
		case 'i':
			switch (lenmod) {
                case LENMOD_SIZE:
                    sn = va_arg(ap, ssize_t);
                break;
                default:
                case LENMOD_LONG_LONG:
                    sn = va_arg(ap, signed long long);
                break;
                case LENMOD_LONG:
                    sn = va_arg(ap, signed long);
                break;
                case LENMOD_NONE:
                    sn = va_arg(ap, signed);
                break;
            }

			len = local_itoa(num, (sn < 0) ? -sn : sn, base, false);
			err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, (sn < 0) ? prfx_neg : NULL, vph, arg);
			break;

		case 'f':
		case 'F':
			dbl = va_arg(ap, double);

            if(fpad == (size_t)-1) {
				fpad = pad;
				pad  = 0;
			}

			if(isinf(dbl)) {
				err |= write_padded("inf", 3, fpad, ' ', plr, NULL, vph, arg);
			} else if (isnan(dbl)) {
				err |= write_padded("nan", 3, fpad, ' ', plr, NULL, vph, arg);
			} else {
				len = local_ftoa(num, dbl, pad ? MIN(pad, DEC_SIZE) : 6);
				err |= write_padded(num, len, fpad, plr ? ' ' : pch, plr, (dbl<0) ? prfx_neg : NULL, vph, arg);
			}
			break;

		case 'H':
			ph     = va_arg(ap, wstk_printf_h *);
			ph_arg = va_arg(ap, void *);

			if(ph) {
				err |= ph(&pf, ph_arg);
			}
			break;

		case 'l':
			++lenmod;
			fm = true;
			break;

		case 'p':
			ptr = va_arg(ap, void *);
			if(ptr) {
				len = local_itoa(num, (unsigned long int)ptr, 16, false);
				err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, prfx_hex, vph, arg);
			} else {
				err |= write_padded(str_nil, sizeof(str_nil) - 1, pad, ' ', plr, NULL, vph, arg);
			}
			break;

		case 'r':
			pl = va_arg(ap, const wstk_pl_t *);
			err |= write_padded(pl ? pl->p : NULL, (pl && pl->p) ? pl->l : 0, pad, ' ', plr, NULL, vph, arg);
			break;

		case 's':
			str = va_arg(ap, const char *);
			err |= write_padded(str, wstk_str_len(str), pad, ' ', plr, NULL, vph, arg);
			break;

		case 'X':
			uc = true;
			/*@fallthrough@*/
		case 'x':
			base = 16;
			/*@fallthrough@*/
		case 'u':
			switch (lenmod) {
                case LENMOD_SIZE:
                    n = va_arg(ap, size_t);
				break;

                default:
                case LENMOD_LONG_LONG:
                    n = va_arg(ap, unsigned long long);
				break;

                case LENMOD_LONG:
                    n = va_arg(ap, unsigned long);
                break;

                case LENMOD_NONE:
                    n = va_arg(ap, unsigned);
				break;
			}

			len = local_itoa(num, n, base, uc);
			err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, NULL, vph, arg);
			break;

		case 'v':
			str = va_arg(ap, char *);
			apl = va_arg(ap, va_list *);

			if(!str || !apl) {
				break;
			}

			err |= wstk_vhprintf(str, *apl, vph, arg);
			break;

		case 'W':
			uc = true;
			/*@fallthrough@*/
		case 'w':
			bptr = va_arg(ap, uint8_t *);
			len = va_arg(ap, size_t);

			len = bptr ? len : 0;
			pch = plr ? ' ' : pch;

			while (!plr && pad-- > (len * 2)) {
				err |= vph(&pch, 1, arg);
			}

			for (i=0; i<len; i++) {
				const uint8_t v = *bptr++;
				uint32_t l = local_itoa(num, v, 16, uc);
				err |= write_padded(num, l, 2, '0', false, NULL, vph, arg);
			}

			while (plr && pad-- > (len * 2)) {
				err |= vph(&pch, 1, arg);
			}

			break;

		case 'z':
			lenmod = LENMOD_SIZE;
			fm = true;
			break;

        case 'j':
            sa = va_arg(ap, wstk_sockaddr_t *);
            if(!sa) { break; }
            if(wstk_sa_inet_ntop(sa, addr, sizeof(addr)) != WSTK_STATUS_SUCCESS) {
                err |= write_padded("?", 1, pad, ' ',
                plr, NULL, vph, arg);
                break;
            }
            err |= write_padded(addr, strlen(addr), pad, ' ', plr, NULL, vph, arg);
            break;

        case 'J':
            sa = va_arg(ap, wstk_sockaddr_t *);
            if(!sa) { break; }
            if(wstk_sa_inet_ntop(sa, addr, sizeof(addr)) != WSTK_STATUS_SUCCESS) {
                err |= write_padded("?", 1, pad, ' ', plr, NULL, vph, arg);
                break;
            }
            wstk_sa_af(sa, &sa_af);
            wstk_sa_port(sa, &sa_port);

#ifdef WSTK_NET_HAVE_INET6
            if(AF_INET6 == sa_af) {
                ch = '[';
                err |= vph(&ch, 1, arg);
            }
#endif
            err |= write_padded(addr, strlen(addr), pad, ' ', plr, NULL, vph, arg);
#ifdef WSTK_NET_HAVE_INET6
            if(AF_INET6 == sa_af) {
                ch = ']';
                err |= vph(&ch, 1, arg);
            }
#endif
            ch = ':';
            err |= vph(&ch, 1, arg);
            len = local_itoa(num, sa_port, 10, false);
            err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, NULL, vph, arg);
        break;

		default:
			if (('0' <= *p) && (*p <= '9')) {
				if (!pad && ('0' == *p)) {
					pch = '0';
				}
				else {
					pad *= 10;
					pad += *p - '0';
				}
				fm = true;
				break;
			}

			ch = '?';

			err |= vph(&ch, 1, arg);
			break;
		}

		if (!fm)
			p0 = p + 1;
	}

	if(!fm && p > p0) {
		err |= vph(p0, p - p0, arg);
	}

	return err;
}


static int print_handler(const char *p, size_t size, void *arg) {
	wstk_pl_t *pl = arg;

	if(size > pl->l) {
        return WSTK_STATUS_MEM_FAIL;
	}

	memcpy((void *)pl->p, p, size);
	wstk_pl_advance(pl, size);

	return WSTK_STATUS_SUCCESS;
}


struct dyn_print {
	char        *str;
	char        *p;
	size_t      l;
	size_t      size;
};

static int print_handler_dyn(const char *p, size_t size, void *arg) {
	struct dyn_print *dp = arg;

	if(size > dp->l - 1) {
		const size_t new_size = MAX(dp->size + size, dp->size * 2);

		if(wstk_mem_realloc((void *)&dp->str, new_size) != WSTK_STATUS_SUCCESS) {
            return WSTK_STATUS_MEM_FAIL;
		}

		dp->l += new_size - dp->size;
		dp->p = dp->str + new_size - dp->l;
		dp->size = new_size;
	}

	memcpy(dp->p, p, size);

	dp->p += size;
	dp->l -= size;

	return WSTK_STATUS_SUCCESS;
}


struct strm_print {
	FILE    *f;
	size_t  n;
};
static int print_handler_stream(const char *p, size_t size, void *arg) {
	struct strm_print *sp = arg;

	if(1 != fwrite(p, size, 1, sp->f)) {
        return WSTK_STATUS_MEM_FAIL;
	}

	sp->n += size;

	return WSTK_STATUS_SUCCESS;
}


/**
 * Print a formatted string to a file stream, using va_list
 *
 * @param stream File stream for the output
 * @param fmt    Formatted string
 * @param ap     Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_vfprintf(FILE *stream, const char *fmt, va_list ap) {
	struct strm_print sp = { 0 };

	if(!stream) {
		return -1;
	}

	sp.f = stream;
	sp.n = 0;

	if(0 != wstk_vhprintf(fmt, ap, print_handler_stream, &sp)) {
        return -1;
	}

	return (int)sp.n;
}


/**
 * Print a formatted string to stdout, using va_list
 *
 * @param fmt Formatted string
 * @param ap  Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_vprintf(const char *fmt, va_list ap) {
	return wstk_vfprintf(stdout, fmt, ap);
}


/**
 * Print a formatted string to a buffer, using va_list
 *
 * @param str  Buffer for output string
 * @param size Size of buffer
 * @param fmt  Formatted string
 * @param ap   Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
	wstk_status_t status = WSTK_STATUS_SUCCESS;
	wstk_pl_t pl;
	int err;

	if(!str || !size) {
        return -1;
	}

	pl.p = str;
	pl.l = size - 1;

	status = wstk_vhprintf(fmt, ap, print_handler, &pl);
	str[size - pl.l - 1] = '\0';

	return (status != WSTK_STATUS_SUCCESS ? -1 : (int)(size - pl.l - 1) );
}


/**
 * Print a formatted string to a dynamically allocated buffer, using va_list
 *
 * @param strp Pointer for output string
 * @param fmt  Formatted string
 * @param ap   Variable-arguments list
 *
 * @return wstk_status_t
 */
wstk_status_t wstk_vsdprintf(char **strp, const char *fmt, va_list ap) {
	wstk_status_t status = WSTK_STATUS_SUCCESS;
	struct dyn_print dp;

	if(!strp) {
        return WSTK_STATUS_INVALID_PARAM;
	}

	dp.size = 16;

    status = wstk_mem_alloc((void *)&dp.str, dp.size, NULL);
	if(status != WSTK_STATUS_SUCCESS) {
        goto out;
	}

	dp.p = dp.str;
	dp.l = dp.size;

	status = wstk_vhprintf(fmt, ap, print_handler_dyn, &dp);
	if(status != WSTK_STATUS_SUCCESS) {
        goto out;
	}

	*dp.p = '\0';
    *strp = dp.str;

out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(dp.str);
    }
	return status;
}


/**
 * Print a formatted string
 *
 * @param pf  Print backend
 * @param fmt Formatted string
 *
 * @return wstk_status_t
 */
wstk_status_t wstk_hprintf(wstk_printf_t *pf, const char *fmt, ...) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
	va_list ap;

	if(!pf) {
        return WSTK_STATUS_INVALID_PARAM;
	}

	va_start(ap, fmt);
	status = wstk_vhprintf(fmt, ap, pf->vph, pf->arg);
	va_end(ap);

	return status;
}


/**
 * Print a formatted string to a file stream
 *
 * @param stream File stream for output
 * @param fmt    Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_fprintf(FILE *stream, const char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = wstk_vfprintf(stream, fmt, ap);
	va_end(ap);

	return n;
}


/**
 * Print a formatted string to stdout
 *
 * @param fmt    Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_printf(const char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = wstk_vprintf(fmt, ap);
	va_end(ap);

	return n;
}


/**
 * Print a formatted string to a buffer
 *
 * @param str  Buffer for output string
 * @param size Size of buffer
 * @param fmt  Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int wstk_snprintf(char *str, size_t size, const char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = wstk_vsnprintf(str, size, fmt, ap);
	va_end(ap);

	return n;
}

/**
 * Print a formatted string to a buffer
 *
 * @param strp Buffer pointer for output string
 * @param fmt  Formatted string
 *
 * @return wstk_status_t
 */
wstk_status_t wstk_sdprintf(char **strp, const char *fmt, ...) {
	wstk_status_t status = WSTK_STATUS_SUCCESS;
	va_list ap;

	va_start(ap, fmt);
	status = wstk_vsdprintf(strp, fmt, ap);
	va_end(ap);

	return status;
}
