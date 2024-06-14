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
    WSTK_DBG_PRINT("Test encoding (wstk-version: %s)", WSTK_VERSION_STR);

    WSTK_DBG_PRINT("System codepage: %d", wstk_system_codepage_id());
    WSTK_DBG_PRINT("System codepage: %s", wstk_system_codepage_name());


    if(1) {
        char *str1 = NULL;
        char *str2 = NULL;
        char *str3 = NULL;

        const char s1[] = "abc-ABC-123";
        const char s2[] = "А Б В Г Д - 123! Hello world!";

        wstk_escape(&str1, s1, sizeof(s1) - 1);
        WSTK_DBG_PRINT("s1-esc=[%s]", str1);

        wstk_unescape(&str3, str1, strlen(str1));
        WSTK_DBG_PRINT("s1-unesc=[%s]", str3);



        wstk_escape(&str2, s2, sizeof(s2) - 1);
        WSTK_DBG_PRINT("s2-esc=[%s]", str2);

        wstk_unescape(&str3, str2, strlen(str2));
        WSTK_DBG_PRINT("s1-unesc=[%s]", str3);
    }
}
