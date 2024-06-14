/**
 ** usage:
 **  curl -v http://127.0.0.1:8080/upload/ -H "Content-Type: multipart/form-data" -F file="@file1.txt"
 **  curl -v http://127.0.0.1:8080/upload/ -H "Authorization: Bearer secret123" -H "Content-Type: multipart/form-data" -F file="@file1.txt"
 **
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

bool upload_on_access(wstk_httpd_sec_ctx_t *ctx) {
    WSTK_DBG_PRINT("---> upload_on_access() session=[%s]", ctx->session);
    return true;
}
bool upload_on_accept(wstk_httpd_sec_ctx_t *ctx, char *filename, size_t size) {
    WSTK_DBG_PRINT("---> upload_on_accept() session=[%s], filename=[%s], size=%d", ctx->session, filename, (int)size);
    return true;
}
void upload_on_complete(wstk_httpd_sec_ctx_t *ctx, char *path) {
    WSTK_DBG_PRINT("---> upload_on_complete() session=[%s], path=[%s]", ctx->session, path);
}


void start_example(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_httpd_t *httpd = NULL;
    wstk_servlet_upload_t *upload_servlet = NULL;
    char *home = NULL;
    char *host = NULL;
    char *charset = "UTF-8";
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port home", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("HTTPD upload servlet example (wstk-version: %s)", WSTK_VERSION_STR);

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

    // upload servlet
    if(wstk_httpd_register_servlet_upload(httpd, "/upload/", &upload_servlet) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet_upload()");
        return;
    }
    if(wstk_servlet_upload_configure(upload_servlet, home, 0) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_upload_configure()");
        return;
    }
    if(wstk_servlet_upload_set_access_handler(upload_servlet, upload_on_access) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_upload_set_access_handler()");
        return;
    }
    if(wstk_servlet_upload_set_accept_handler(upload_servlet, upload_on_accept) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_upload_set_accept_handler()");
        return;
    }
    if(wstk_servlet_upload_set_complete_handler(upload_servlet, upload_on_complete) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_upload_set_accept_handler()");
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
