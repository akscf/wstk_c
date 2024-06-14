/**
 **
 ** (C)2024 aks
 **/
#include <wstk-log.h>
#include <wstk-mem.h>
#include <wstk-mutex.h>

static bool _log_init = false;
static bool _use_lock = false;
static bool _cfg_ok = false;
static wstk_log_mode_e _mode;
static wstk_mutex_t *io_mutex;

wstk_status_t wstk_pvt_log_init() {
    wstk_status_t st;

    if(_log_init) {
        return WSTK_STATUS_SUCCESS;
    }
    if(!io_mutex) {
        st = wstk_mutex_create(&io_mutex);
    }

    if(st == WSTK_STATUS_SUCCESS) {
        _log_init = true;
    }

    _mode = WSTK_LOG_STDERR;

#ifdef WSTK_OS_WIN
        /* for win console */
        _use_lock = true;
#endif

    return WSTK_STATUS_SUCCESS;
}

wstk_status_t wstk_pvt_log_shutdown() {
    if(!_log_init) {
        return WSTK_STATUS_SUCCESS;
    }

    if(io_mutex) {
        io_mutex = wstk_mem_deref(io_mutex);
    }

#ifdef WSTK_HAVE_SYSLOG
    if(_mode == WSTK_LOG_SYSLOG) {
        closelog();
    }
#endif

    _log_init = false;
    return WSTK_STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Configure logger
 * should only be called once
 *
 * @param mode  - log mode
 * @param name  - null or syslogname or filename
 *
 **/
wstk_status_t wstk_log_configure(wstk_log_mode_e mode, char *name) {
    wstk_status_t st = WSTK_STATUS_SUCCESS;

    if(_cfg_ok) {
        return WSTK_STATUS_FALSE;
    }

    _mode = mode;
    _cfg_ok = true;

    if(mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        openlog(name, LOG_PID | LOG_NDELAY, LOG_DAEMON);
#else
        st = WSTK_STATUS_UNSUPPORTED;
#endif
    } else if(mode == WSTK_LOG_FILE) {
        if(!name) {
            st = WSTK_STATUS_INVALID_PARAM;
        } else {
            if(freopen(name, "w", stderr) == NULL) {
                log_error("Unable to redirect stderr to %s", name);
            }
        }
    }

    if(st != WSTK_STATUS_SUCCESS) {
        _mode = WSTK_LOG_STDERR;
    }

    return st;
}

void wstk_log_debug(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_DEBUG, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
    va_end(ap);
}

void wstk_log_vdebug(const char *fmt, va_list ap) {
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_DEBUG, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
}

void wstk_log_notice(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_NOTICE, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
    va_end(ap);
}

void wstk_log_vnotice(const char *fmt, va_list ap) {
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_NOTICE, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
}

void wstk_log_error(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_ERR, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
    va_end(ap);
}

void wstk_log_verror(const char *fmt, va_list ap) {
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_ERR, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
}

void wstk_log_warn(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_WARNING, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
    va_end(ap);
}

void wstk_log_vwarn(const char *fmt, va_list ap) {
    if(_mode == WSTK_LOG_SYSLOG) {
#ifdef WSTK_HAVE_SYSLOG
        vsyslog(LOG_WARNING, fmt, ap);
#endif
    } else {
        if(_use_lock && io_mutex) { wstk_mutex_lock(io_mutex); }
        vfprintf(stderr, fmt, ap);
        if(_use_lock && io_mutex) { wstk_mutex_unlock(io_mutex); }
    }
}
