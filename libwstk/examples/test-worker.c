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



static wstk_mutex_t *mutex;
static uint32_t counter = 0;

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
static void my_worker_handler(wstk_worker_t *worker, void *qdata) {
    char *str = (char *)qdata;
    uint32_t cnt = 0;

    wstk_mutex_lock(mutex);
    cnt = counter;
    counter++;
    wstk_mutex_unlock(mutex);

    WSTK_DBG_PRINT("my_worker_handler() counter=%d, str=%s", cnt, str);
    wstk_mem_deref(str);

    wstk_msleep(1000);
}


void start_example(int argc, char **argv) {
    wstk_worker_t *worker = NULL;

    WSTK_DBG_PRINT("Test worker (wstk-version: %s)", WSTK_VERSION_STR);

    if(wstk_worker_create(&worker, 3, 10, 128, 15, my_worker_handler) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_worker_create()");
        return;
    }

    if(wstk_mutex_create(&mutex) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_mutex_create()");
        return;
    }

    WSTK_DBG_PRINT("Wait for ready...");
    while(!wstk_worker_is_ready(worker)) {
        wstk_msleep(1000);
    }

    WSTK_DBG_PRINT("Performing jobs...");
    for(int i=0; i < 40; i++) {
        char *str = NULL;
        wstk_sdprintf(&str, "Test data %d", i);

        if(wstk_worker_perform(worker, str) != WSTK_STATUS_SUCCESS) {
            WSTK_DBG_PRINT("ERR: wstk_worker_perform()");
        }
    }

    WSTK_DBG_PRINT("Wait for all jobs done...");
    while(counter < 40) {
        wstk_msleep(1000);
    }

    WSTK_DBG_PRINT("Stopping worker...");
    wstk_mem_deref(worker);
    WSTK_DBG_PRINT("Worker stopped");
}
