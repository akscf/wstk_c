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
            printf("ctrl-c\n");
        break;
        case CTRL_BREAK_EVENT:
            int_handler(0);
            printf("break\n");
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
static void my_thread_proc(wstk_thread_t *th, void *udata) {
    char *str = (char *)udata;

    WSTK_DBG_PRINT("Thread active: str=[%s]", str);

    WSTK_DBG_PRINT("Sleep 3s...");
    wstk_msleep(3000);
    WSTK_DBG_PRINT("Sleep 3s...done");

    wstk_mem_deref(udata);
}

void start_example(int argc, char **argv) {
    wstk_thread_t *thr = NULL;
    char *str = NULL;

    WSTK_DBG_PRINT("Test threads (wstk-version: %s)", WSTK_VERSION_STR);
    WSTK_DBG_PRINT("With the thread control structure");
    WSTK_DBG_PRINT("-----------------------------------------------------------");

    wstk_sdprintf(&str, "Hello world!");

    if(wstk_thread_create(&thr, my_thread_proc, str, 0) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_thread_create()");
        return;
    }

    while(true) {
        if(globa_break) {
            break;
        }

        if(wstk_thread_is_finished(thr)) {
            WSTK_DBG_PRINT("Thread finished");
            break;
        }

        WSTK_DBG_PRINT("main loop");
        wstk_msleep(1000);
    }

    /* destroying control structure */
    wstk_mem_deref(thr);
}
