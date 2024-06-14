/**
 **
 ** (C)2024 aks
 **/
#include <wstk.h>

static bool globa_break = false;
static void int_handler(int dummy) { globa_break = true; }
static void start_example(int argc, char **argv);

#ifdef WSTK_OS_WIN
static BOOL WINAPI cons_handler(DWORD type) {
    switch(type) {
        case CTRL_C_EVENT:
            int_handler(0);
        break;
        case CTRL_BREAK_EVENT:
            int_handler(0);
        break;
    }
    return TRUE;
}
#endif

int main(int argc, char **argv) {
#ifndef WSTK_OS_WIN
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, int_handler);
#else
    if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE)cons_handler, TRUE)) {
        WSTK_DBG_PRINT("ERROR: SetConsoleCtrlHandler()");
        return EXIT_FAILURE;
    }
#endif

    if(wstk_core_init() != WSTK_STATUS_SUCCESS) {
        exit(1);
    }

    setbuf(stderr, NULL);
    setbuf(stdout, NULL);

    start_example(argc, argv);

    wstk_core_shutdown();
    exit(0);
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// example code
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void start_example(int argc, char **argv) {
    char *mode = NULL;

    mode = (argc > 1 ? argv[1] : NULL);

    log_debug("usage: %s syslog|file", argv[0]);
    log_debug("\n\n\n");

    /* by default stderr */
    log_debug("Test debug message (console)");
    log_notice("Test notice message (console)");
    log_error("Test error message (console)");
    log_warn("Test warn message (console)");
    log_debug("\n\n\n");

    /* redirect to file */
    if(wstk_str_equal(mode, "file", false)) {
#ifdef WSTK_WIN_OS
        log_debug("*** redirect to: c:/tmp/wstk.log");
        wstk_log_configure(WSTK_LOG_STDERR, "c:/tmp/wstk.log");
#else
        log_debug("*** redirect to: /tmp/wstk.log");
        wstk_log_configure(WSTK_LOG_FILE, "/tmp/wstk.log");
#endif

        log_debug("Test debug message (file)");
        log_notice("Test notice message (file)");
        log_error("Test error message (file)");
        log_warn("Test warn message (file)");
    }

    /* redirect to syslog */
    if(wstk_str_equal(mode, "syslog", false)) {
        log_debug("*** redirect to: SYSLOG");

        wstk_log_configure(WSTK_LOG_SYSLOG, "wstkd");

        log_debug("Test debug message (syslog)");
        log_notice("Test notice message (syslog)");
        log_error("Test error message (syslog)");
        log_warn("Test warn message (syslog)");
    }
}
