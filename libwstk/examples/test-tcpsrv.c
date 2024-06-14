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
static void my_tcp_handler(wstk_tcp_srv_conn_t *conn, wstk_mbuf_t *mbuf) {
    wstk_status_t st;
    wstk_socket_t *sock = NULL;
    uint32_t delay = 0;
    char tbuff[128] = {0};
    char *str1 = "Hello world 1\n\n";
    char *str2 = "Hello world 2\n\n";
    char *str3 = "Hello world 3\n\n";
    char *str = str1;
    bool close_conn = true;

    WSTK_DBG_PRINT("***** my_tcp_handler() conn=%p", conn);

    if(wstk_regex((char *)mbuf->buf, mbuf->end, "/test2/") == WSTK_STATUS_SUCCESS) {
        delay = wstk_rand_range(0, 10000);
        str = str2;
    }
    if(wstk_regex((char *)mbuf->buf, mbuf->end, "keep-alive") == WSTK_STATUS_SUCCESS) {
        close_conn = false;
        str = str3;
    }

    if((st = wstk_tcp_srv_conn_socket(conn, &sock)) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("***** ERROR, conn=%p, st=%d", conn, (int)st);
        //wstk_tcp_srv_conn_close(conn);
        return;
    } else {
        if(close_conn) {
            wstk_tcp_srv_conn_close(conn);
        }
        wstk_time_to_str_rfc822(0, (char *)tbuff, sizeof(tbuff));
        wstk_tcp_printf(sock, "HTTP/1.1 200 OK\r\n"
                         "Server: apache/1.1\r\n"
                        "Date: %s\r\n"
                        "Last-Modified: %s\r\n"
                         "Connection: close\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n"
                         "%s",
                        (char *)tbuff, (char *)tbuff,
                         strlen(str), str
        );
    }

    if(delay) {
        WSTK_DBG_PRINT("***** SLEEP-START delay=%u (conn=%p)", delay, conn);
        wstk_msleep(delay);
        WSTK_DBG_PRINT("***** SLEEP-DONE  delay=%u (conn=%p)", delay, conn);
    }
}

void start_example(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_tcp_srv_t *srv = NULL;
    char *host = NULL;
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("Test tcp-server (wstk-version: %s)", WSTK_VERSION_STR);

    host = argv[1];
    port = atoi(argv[2]);

    if(wstk_sa_set_str(&sa, host, port) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_sa_set_str()");
        return;
    }

    wstk_printf("Starting TCP server (%J)....\n", (wstk_sockaddr_t *)&sa);
    if(wstk_tcp_srv_create(&srv, &sa, 100, 60, 8192, my_tcp_handler) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_tcp_srv_create()");
        return;
    }

    if(wstk_tcp_srv_start(srv) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_tcp_srv_start()");
        return;
    }
    wstk_printf("Server statred on: %J\nUse ctrl+c to terminate one\n-----------------------------------------------\n", (wstk_sockaddr_t *)&sa);

    while(true) {
        if(globa_break) {
            break;
        }
        wstk_msleep(1000);
    }

    WSTK_DBG_PRINT("Stopping TCP server...");
    wstk_mem_deref(srv);
    WSTK_DBG_PRINT("Stopping TCP server...DONE");
}
