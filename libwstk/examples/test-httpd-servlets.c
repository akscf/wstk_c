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
void my_auth_handler(wstk_httpd_auth_request_t *req, wstk_httpd_auth_response_t *rsp) {

    WSTK_DBG_PRINT("---> my_auth_handler() login=[%s], password=[%s], token=[%s], session=[%s]", req->login, req->password, req->token, req->session);

    rsp->permitted = true;
}

void my_servlet_handler1(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    WSTK_DBG_PRINT("---> my_servlet_handler1() conn=%p, msg=%p, udata=%p", conn, msg, udata);

    wstk_http_msg_dump(msg);

    wstk_tcp_srv_conn_close(conn->tcp_conn);
    wstk_httpd_creply(conn, 200, NULL, "text/html", "<html><body>Hello from servlet #1</body></html>\n");
}

void my_servlet_handler2(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    WSTK_DBG_PRINT("---> my_servlet_handler2() conn=%p, msg=%p, udata=%p", conn, msg, udata);

    wstk_tcp_srv_conn_close(conn->tcp_conn);
    wstk_httpd_creply(conn, 200, NULL, "text/html", "<html><body>Hello from servlet #2</body></html>\n");

    WSTK_DBG_PRINT("SLEEP FOR 5sec (conn=%p)", conn);
    wstk_msleep(5000);
    WSTK_DBG_PRINT("SLEEP - DONE (conn=%p)", conn);
}

void start_example(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_httpd_t *httpd = NULL;
    char *home = NULL;
    char *host = NULL;
    char *charset = "UTF-8";
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port home", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("HTTPD servlets example (wstk-version: %s)", WSTK_VERSION_STR);

    host = argv[1];
    port = atoi(argv[2]);

    if(argc > 2) {
        home = argv[3];
    }

    if(!home) {
#ifdef WSTK_OS_WIN
        home = "c:/dev/www";
#else
        home = "/var/www";
#endif
    }

    if(wstk_sa_set_str(&sa, host, port) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_sa_set_str()");
        return;
    }

    wstk_printf("Starting HTTP server (%J)....\n", (wstk_sockaddr_t *)&sa);
    if(wstk_httpd_create(&httpd, &sa, 100, 60, charset, home, "index.html", true) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_create()");
        return;
    }

    if((wstk_httpd_set_ident(httpd, "MyServer/1.0")) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_set_ident()");
        return;
    }

    if((wstk_httpd_set_authenticator(httpd, my_auth_handler, true)) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_set_authenticator()");
        return;
    }


    if(wstk_httpd_register_servlet(httpd, "/test1/", my_servlet_handler1, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet()");
        return;
    }
    if(wstk_httpd_register_servlet(httpd, "/test2/", my_servlet_handler2, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet()");
        return;
    }

    // start httpd
    if(wstk_httpd_start(httpd) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("SERVER FAILED2!");
        return;
    }

    wstk_printf("Server statred on: %J\nUse ctrl+c to terminate one\n-----------------------------------------------\n", (wstk_sockaddr_t *)&sa);

    while(true) {
        if(globa_break) {
            break;
        }
        wstk_msleep(1000);
    }

    WSTK_DBG_PRINT("Stopping HTTPD server...");
    wstk_mem_deref(httpd);
    WSTK_DBG_PRINT("Stopping HTTPD server...DONE");

}
