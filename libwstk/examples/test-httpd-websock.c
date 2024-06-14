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

bool my_websock_on_accept(wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *ctx) {
    WSTK_DBG_PRINT("--> websock_on_accept() conn=%p, ctx=%p", conn, ctx);

    return ctx->permitted;
}

void my_websock_on_close(wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *ctx) {
    WSTK_DBG_PRINT("--> websock_on_close() conn=%p, sec_ctx=%p", conn, ctx);
}

void my_websock_on_message(wstk_servlet_websock_conn_t *conn, wstk_mbuf_t *mbuf) {
    char *str = NULL;

    WSTK_DBG_PRINT("--> websock_on_message() ws-opcode=%d, ws-len=%d, sec-ctx=%p, sec-token=[%s]", conn->header->opcode, (int)conn->header->len, conn->sec_ctx, conn->sec_ctx->token);

    wstk_mbuf_strdup(mbuf, &str, conn->header->len);
    WSTK_DBG_PRINT("--> ws-echo: [%s]", str);

    wstk_servlet_websock_send(conn->http_conn, WEBSOCK_TEXT, "%s", str);
    wstk_mem_deref(str);
}

void start_example(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_httpd_t *httpd = NULL;
    wstk_servlet_websock_t *websock_servlet = NULL;
    char *home = NULL;
    char *host = NULL;
    char *charset = "UTF-8";
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port home", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("HTTPD websock example (wstk-version: %s)", WSTK_VERSION_STR);

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

    // websocket
    if(wstk_httpd_register_servlet_websock(httpd, "/ws/", &websock_servlet) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet_websock()");
        return;
    }
    wstk_servlet_websock_set_on_close(websock_servlet, my_websock_on_close);
    wstk_servlet_websock_set_on_accept(websock_servlet, my_websock_on_accept);
    wstk_servlet_websock_set_on_message(websock_servlet, my_websock_on_message);


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
