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
void list_foreach_cb(uint32_t pos, void *data) {
    WSTK_DBG_PRINT("foreach: idx=[%d] data=[%s]", pos, data);
}

void list_clear_cb(uint32_t pos, void *data) {
    WSTK_DBG_PRINT("clear: idx=[%d] data=[%s]", pos, data);
}

bool list_find_cb(uint32_t pos, void *data) {
    WSTK_DBG_PRINT("find: idx=[%d] data=[%s]", pos, data);
    return wstk_str_equal(data, "item2", false);
}

void start_example(int argc, char **argv) {
    wstk_list_t *list = NULL;

    WSTK_DBG_PRINT("Test hastable (wstk-version: %s)", WSTK_VERSION_STR);

    if(wstk_list_create(&list) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_hash_init()");
        return;
    }

    wstk_list_add_tail(list, "item0", NULL);
    wstk_list_add_tail(list, "item1", NULL);
    wstk_list_add_tail(list, "item2", NULL);
    wstk_list_add_tail(list, "item3", NULL);

    /*wstk_list_add_head(list, "item0", NULL);
    wstk_list_add_head(list, "item1", NULL);
    wstk_list_add_head(list, "item2", NULL);
    wstk_list_add_head(list, "item3", NULL);*/

    //WSTK_DBG_PRINT("list_size=%d", wstk_list_get_size(list) );

    for(int i = 0; i < wstk_list_get_size(list); i++) {
        WSTK_DBG_PRINT("list [%d]=[%s]", i, wstk_list_get(list, i));
    }

    WSTK_DBG_PRINT("---------------------------------------------------");
    //wstk_list_add_head(list, "itemX", NULL);  // OK
    wstk_list_add(list, 1, "itemX", NULL);    // OK
    //wstk_list_add_tail(list, "itemX", NULL);  // OK

    //wstk_list_del(list, 0); // OK
    wstk_list_del(list, 1); // OK
    //wstk_list_del(list, 3); // OK

    WSTK_DBG_PRINT("---------------------------------------------------");
    wstk_list_foreach(list, list_foreach_cb);


    WSTK_DBG_PRINT("---------------------------------------------------");
    wstk_list_find_t fnd = wstk_list_find(list, list_find_cb);
    if(fnd.found) {
        WSTK_DBG_PRINT("item found: pos=%d, data=%s",fnd.pos, fnd.data);
    }


    WSTK_DBG_PRINT("---------------------------------------------------");
    wstk_list_clear(list, list_clear_cb);


    wstk_mem_deref(list);
}
