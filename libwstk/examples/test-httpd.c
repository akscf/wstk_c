/**
 ** all in one
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

wstk_servlet_jsonrpc_handler_result_t *my_service_handler(wstk_httpd_sec_ctx_t *ctx, const char *method, const cJSON *params) {
    /*if(1) {
        char *ttt=NULL;
        ttt = cJSON_PrintUnformatted(params);
        if(ttt) {
            WSTK_DBG_PRINT("params=[%s]", ttt);
            free(ttt);
        }
    }*/
    if(wstk_str_equal(method, "str", false))  {
        cJSON *js_obj = cJSON_CreateString("This is json string");
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "str2", false))  {
        cJSON *js_obj = cJSON_CreateString("Тестовая трока (utf-8)");
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "date", false))  {
        char tmp[128] = {0};
        wstk_time_to_str_json(0, tmp, sizeof(tmp));
        cJSON *js_obj = cJSON_CreateString((char *)tmp);
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "int", false))  {
        cJSON *js_obj = cJSON_CreateNumber(12345);
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "float", false))  {
        cJSON *js_obj = cJSON_CreateNumber(123.456);
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "true", false))  {
        cJSON *js_obj = cJSON_CreateTrue();
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "false", false))  {
        cJSON *js_obj = cJSON_CreateFalse();
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "list", false))  {
        cJSON *js_obj = cJSON_CreateArray();
        cJSON_AddItemToArray(js_obj, cJSON_CreateString("str1"));
        cJSON_AddItemToArray(js_obj, cJSON_CreateString("str2"));
        cJSON_AddItemToArray(js_obj, cJSON_CreateNumber(123456));
        cJSON_AddItemToArray(js_obj, cJSON_CreateNumber(123.456));
        cJSON_AddItemToArray(js_obj, cJSON_CreateTrue());
        cJSON_AddItemToArray(js_obj, cJSON_CreateFalse());
        return wstk_jsonrpc_ok(js_obj);
    }
    if(wstk_str_equal(method, "obj", false))  {
        cJSON *js_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(js_obj, "class", cJSON_CreateString("models.user"));
        cJSON_AddItemToObject(js_obj, "id", cJSON_CreateNumber(123456));
        cJSON_AddItemToObject(js_obj, "name", cJSON_CreateString("Test user"));
        cJSON_AddItemToObject(js_obj, "enabled", cJSON_CreateTrue());
        return wstk_jsonrpc_ok(js_obj);
    }

    if(wstk_str_equal(method, "sum", false))  {
        cJSON *js_obj = NULL, *arg1 = NULL, *arg2 = NULL;
        int isum = 0;
        double fsum = 0;

        if(cJSON_GetArraySize(params) < 2) {
            return wstk_jsonrpc_err(RPC_ERROR_PARAMETR_MISMATCH, "Wrong arguments");
        }
        arg1 = cJSON_GetArrayItem(params, 0);
        arg2 = cJSON_GetArrayItem(params, 1);

        if(!cJSON_IsNumber(arg1) || !cJSON_IsNumber(arg2)) {
            return wstk_jsonrpc_err(RPC_ERROR_PARAMETR_MISMATCH, "Wrong arguments (2)");
        }

        isum = arg1->valueint + arg2->valueint;
        fsum = arg1->valuedouble + arg2->valuedouble;

        js_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(js_obj, "sum1", cJSON_CreateNumber(isum));
        cJSON_AddItemToObject(js_obj, "sum2", cJSON_CreateNumber(fsum));

        return wstk_jsonrpc_ok(js_obj);
    }

    if(wstk_str_equal(method, "delay", false))  {
        WSTK_DBG_PRINT("delay-start, ctx=%p, ctx-sid=[%s]", ctx, ctx->session);
        wstk_msleep(10000);
        WSTK_DBG_PRINT("delay-end, ctx=%p, ctx-sid=[%s]", ctx, ctx->session);
        return NULL;
    }

    return wstk_jsonrpc_err(RPC_ERROR_METHOD_NOT_FOUND, method);
}


bool my_websock_on_accept(wstk_http_conn_t *conn, wstk_httpd_sec_ctx_t *ctx) {
    WSTK_DBG_PRINT("--> websock_on_accept() conn=%p, ctx=%p", conn, ctx);
    return true;
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
    wstk_servlet_jsonrpc_t *jsrpc_servlet = NULL;
    wstk_servlet_websock_t *websock_servlet = NULL;
    wstk_servlet_upload_t *upload_servlet = NULL;
    char *home = NULL;
    char *host = NULL;
    char *charset = "UTF-8";
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port home", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("HTTPD example (wstk-version: %s)", WSTK_VERSION_STR);

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


    // test servlets
    if(wstk_httpd_register_servlet(httpd, "/test1/", my_servlet_handler1, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet()");
        return;
    }
    if(wstk_httpd_register_servlet(httpd, "/test2/", my_servlet_handler2, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet()");
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


    // json-rpc
    if(wstk_httpd_register_servlet_jsonrpc(httpd, "/rpc/", &jsrpc_servlet) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_httpd_register_servlet_jsonrpc()");
        return;
    }
    if(wstk_servlet_jsonrpc_register_service(jsrpc_servlet, "MyService1", my_service_handler, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_jsonrpc_register_service()");
        return;
    }
    if(wstk_servlet_jsonrpc_register_service(jsrpc_servlet, "MyService2", my_service_handler, NULL, false) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_servlet_jsonrpc_register_service()");
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
