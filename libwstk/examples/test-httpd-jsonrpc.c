/**
 ** usage:
 **  curl -d '{"id":1,"service":"MyService1","method":"str","params":[]}' -H "Content-Type: application/json; charset=UTF-8" -X POST http://127.0.0.1:8080/rpc/
 **  curl -d '{"id":1,"service":"MyService1","method":"sum","params":[1,2]}' -H "Content-Type: application/json; charset=UTF-8" -X POST http://127.0.0.1:8080/rpc/
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

wstk_servlet_jsonrpc_handler_result_t *my_service_handler(wstk_httpd_sec_ctx_t *ctx, const char *method, const cJSON *params) {
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
        wstk_msleep(10000);
        return NULL;
    }

    return wstk_jsonrpc_err(RPC_ERROR_METHOD_NOT_FOUND, method);
}


void start_example(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_httpd_t *httpd = NULL;
    wstk_servlet_jsonrpc_t *jsrpc_servlet = NULL;
    char *home = NULL;
    char *host = NULL;
    char *charset = "UTF-8";
    uint32_t port = 0;

    if(!argc || argc < 2) {
        WSTK_DBG_PRINT("usage: %s ip port home", argv[0]);
        return;
    }

    WSTK_DBG_PRINT("HTTPD json-rpc-service example (wstk-version: %s)", WSTK_VERSION_STR);

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
