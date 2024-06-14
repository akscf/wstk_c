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
    float f1 = 0.123;
    double f2 = 123.456;
    double f3 = NAN;
    double f4 = INFINITY;
    uint8_t i8 = 0xAB;
    uint16_t i16 = 0xABCD;
    uint32_t i32 = 0xABCDEF00;
    uint64_t i64 = 0xABCDEEF00ABACAD;
    const char *str= "Hello 123";
    char *tmp = NULL;
    cJSON *jsf1 = NULL;
    cJSON *jsf2 = NULL;
    cJSON *jsf3 = NULL;
    cJSON *jsf4 = NULL;


    WSTK_DBG_PRINT("Test FMT (wstk-version: %s)", WSTK_VERSION_STR);

    printf("printf() f1=%f\n", f1);
    printf("printf() f2=%f\n", f2);
    printf("printf() f3=%f (nan)\n", f3);
    printf("printf() f4=%f (inf)\n", f4);

    WSTK_DBG_PRINT("---------------------------------------------");

    wstk_printf("wstk_printf() f1=%f\n", f1);
    wstk_printf("wstk_printf() f2=%f\n", f2);
    wstk_printf("wstk_printf() f3=%f (nan)\n", f3);
    wstk_printf("wstk_printf() f4=%f (inf)\n", f4);

    WSTK_DBG_PRINT("---------------------------------------------");

    jsf1 = cJSON_CreateNumber(f1);
    jsf2 = cJSON_CreateNumber(f2);
    jsf3 = cJSON_CreateNumber(f3);
    jsf4 = cJSON_CreateNumber(f4);

    wstk_printf("cJSON f1=[%s]\n", cJSON_Print(jsf1));
    wstk_printf("cJSON f2=[%s]\n", cJSON_Print(jsf2));
    wstk_printf("cJSON f3=[%s] (nan)\n", cJSON_Print(jsf3));
    wstk_printf("cJSON f4=[%s] (inf)\n", cJSON_Print(jsf4));


    WSTK_DBG_PRINT("---------------------------------------------");
    wstk_printf("i8=%zu (0x%x)\n", i8, i8);
    wstk_printf("i16=%zu (0x%x)\n", i16, i16);
    wstk_printf("i32=%zu (0x%x)\n", i32, i32);
    wstk_printf("i64=%llu (0x%x)\n", i64, i64);
    wstk_printf("str=[%s]\n", str);

}

