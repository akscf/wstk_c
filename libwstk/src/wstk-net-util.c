/**
 ** based on libre
 **
 ** (C)2019 aks
 **/
#include <wstk-core.h>
#include <wstk-log.h>
#include <wstk-str.h>
#include <wstk-fmt.h>
#include <wstk-mem.h>
#include <wstk-net.h>

#define NS_IN6ADDRSZ     16      /**< IPv6 T_AAAA */
#define NS_INT16SZ       2       /**< #/bytes of data in a unsigned int16_t */

#define NS_INADDRSZ      4       /**< IPv4 T_A */
#define NS_IN6ADDRSZ     16      /**< IPv6 T_AAAA */
#define NS_INT16SZ       2       /**< #/bytes of data in a unsigned int16_t */

/**
 ** 1 if ok else 0
 **/
static int xxx_inet_ntop4(const uint8_t *src, char *dst, size_t size) {
    if(wstk_snprintf(dst, size, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]) < 0) {
        dst[size-1] = 0;
        return 0; // ENOSPC;
    }
    return 1;
}

/*
 * inet_pton4(src, dst)
 *      like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *      wstk_success if `src' is a valid dotted quad, else 0.
 * notice:
 *      does not touch `dst' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int xxx_inet_pton4(const char *src, unsigned char *dst) {
    static const char digits[] = "0123456789";
    int saw_digit, octets, ch;
    unsigned char tmp[NS_INADDRSZ], *tp;

    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;

    while((ch = *src++) != '\0') {
        const char *pch;
        if((pch = strchr(digits, ch)) != NULL) {
            unsigned int newVal = (unsigned int) (*tp * 10 + (pch - digits));

            if(newVal > 255) {
                return 0;
            }

            *tp = newVal;
            if(!saw_digit) {
                if(++octets > 4) { return 0; }
                saw_digit = 1;
            }
        } else if(ch == '.' && saw_digit) {
            if(octets == 4) { return 0; }
            *++tp = 0;
            saw_digit = 0;
        } else {
            return 0;
        }
    }

    if(octets < 4) {
        return 0;
    }

    memcpy(dst, tmp, NS_INADDRSZ);

    return 1;
}



#ifdef WSTK_NET_HAVE_INET6
/*
 * convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 * 1 if ok else 0
*/
static int xxx_inet_ntop6(const uint8_t *src, char *dst, size_t size) {
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct { int base, len; } best, cur;
	unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for (i = 0; i < NS_IN6ADDRSZ; i++) {
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	}

	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;

	for(i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1) {
				cur.base = i, cur.len = 1;
			} else {
				cur.len++;
			}
		} else {
			if(cur.base != -1) {
				if (best.base == -1 || cur.len > best.len) { best = cur; }
				cur.base = -1;
			}
		}
	}
	if(cur.base != -1) {
		if(best.base == -1 || cur.len > best.len) {
            best = cur;
        }
	}
	if(best.base != -1 && best.len < 2) {
        best.base = -1;
    }

	/*
	 * Format the result.
	 */
	tp = tmp;
	for(i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		/* Are we inside the best run of 0x00's? */
		if(best.base != -1 && i >= best.base && i < (best.base + best.len)) {
			if(i == best.base) {
                *tp++ = ':';
            }
			continue;
		}

		/* Are we following an initial run of 0x00s or any real hex?*/
		if(i != 0) {
			*tp++ = ':';
		}

		/* Is this address an encapsulated IPv4? */
		if(i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if(!xxx_inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp))) {
                return 0;
			}

			tp += strlen(tp);
			break;
		}
		tp += sprintf(tp, "%x", words[i]);
	}

	/* Was it a trailing run of 0x00's? */
	if(best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ)) {
		*tp++ = ':';
	}
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if((size_t)(tp - tmp) > size) {
		return 0; // ENOSPC;
	}

	strcpy(dst, tmp);
	return 1;
}

/*
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */
static int xxx_inet_pton6(const char *src, u_char *dst) {
	static const char xdigits_l[] = "0123456789abcdef",
    xdigits_u[] = "0123456789ABCDEF";
	u_char tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, saw_xdigit;
	unsigned int val;

	memset((tp = tmp), '\0', NS_IN6ADDRSZ);
	endp = tp + NS_IN6ADDRSZ;
	colonp = NULL;

	/* Leading :: requires some special handling. */
	if(*src == ':') {
		if(*++src != ':') {
			return 0;
		}
	}

	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if((pch = strchr((xdigits = xdigits_l), ch)) == NULL) {
			pch = strchr((xdigits = xdigits_u), ch);
		}
		if (pch != NULL) {
			val <<= 4;
			val |= (unsigned int)(pch - xdigits);
			if (val > 0xffff) { return 0; }
			saw_xdigit = 1;

			continue;
		}
		if (ch == ':') {
			curtok = src;
			if(!saw_xdigit) {
				if(colonp) { return 0; }
				colonp = tp;
				continue;
			}
			if(tp + NS_INT16SZ > endp) {
                return 0;
            }

			*tp++ = (u_char) (val >> 8) & 0xff;
			*tp++ = (u_char) val & 0xff;
			saw_xdigit = 0;
			val = 0;

			continue;
		}
		if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) && xxx_inet_pton4(curtok, tp) > 0) {
			tp += NS_INADDRSZ;
			saw_xdigit = 0;
			break;	/* '\0' was seen by inet_pton4(). */
		}
		return 0;
	}

	if (saw_xdigit) {
		if(tp + NS_INT16SZ > endp) {
			return 0;
		}
		*tp++ = (u_char) (val >> 8) & 0xff;
		*tp++ = (u_char) val & 0xff;
	}

	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = (int)(tp - colonp);
		int i;

		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}

	if(tp != endp) {
		return 0;
	}

	memcpy(dst, tmp, NS_IN6ADDRSZ);
	return 1;
}

#endif

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 *
 * @result >0 OK
 **/
int wstk_net_ntop(int af, const void *src, char *dst, size_t size) {
    if(!src || !dst) {
        return 0;
    }

    switch(af) {
        case AF_INET:
            return xxx_inet_ntop4(src, dst, size);

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            return xxx_inet_ntop6(src, dst, size);
#endif
        default:
            return -1;
    }
}

/**
 *
 * @result >0 OK
 **/
int wstk_net_pton(int af, const char *src, void *dst) {
    if(!src || !dst) {
        return 0;
    }

    switch(af) {
        case AF_INET:
            return xxx_inet_pton4(src, (unsigned char *)dst);

#ifdef WSTK_NET_HAVE_INET6
        case AF_INET6:
            return xxx_inet_pton6(src, (unsigned char *)dst);
#endif
        default:
            return -1;
    }
}

