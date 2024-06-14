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
static int str_hash(int max_items) {
    wstk_hash_t *table = NULL;

    if(wstk_hash_init(&table) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_hash_init()");
        return -1;
    }

    WSTK_DBG_PRINT("insert key1...");
    wstk_hash_insert_ex(table, "key1", wstk_str_dup("VAL-1_0"), true);
    WSTK_DBG_PRINT("key1=%s", wstk_hash_find(table, "key1"));

    WSTK_DBG_PRINT("replace key1...");
    wstk_hash_insert_ex(table, "key1", wstk_str_dup("VAL-1_1"), true);
    WSTK_DBG_PRINT("key1=%s", wstk_hash_find(table, "key1"));

    WSTK_DBG_PRINT("delete key1...");
    wstk_hash_delete(table, "key1");
    WSTK_DBG_PRINT("key1=%s", wstk_hash_find(table, "key1"));

    WSTK_DBG_PRINT("table size=%d", wstk_hash_size(table));
    // ------------------------------------------------------------------------

    WSTK_DBG_PRINT("insert %d items...", max_items);
    for(int i=0; i < max_items; i++) {
        char key[255] = {0};
        char *val = NULL;

        wstk_snprintf(key, sizeof(key), "key-%i", i);
        wstk_sdprintf(&val, "value__%i", i);

        //wstk_hash_insert(table, (char *)key, val);
        wstk_hash_insert_ex(table, (char *)key, val, true);
    }

    WSTK_DBG_PRINT("table size=%d", wstk_hash_size(table));

    if(!wstk_hash_is_empty(table)) {
        wstk_hash_index_t *hidx = NULL;
        char *key=NULL, *val=NULL;

        for(hidx = wstk_hash_first_iter(table, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
            wstk_hash_this(hidx, (void *)&key, NULL, (void *)&val);

            if(!key || !val) {
                WSTK_DBG_PRINT("corrupted item: key=%p, val=%p, idx=%p", key, val, hidx);
                break;
            }
            //WSTK_DBG_PRINT("key=%s, val=%s", key, val);
        }
    }

    wstk_mem_deref(table);
    return 0;
}
static int int_hash(int max_items) {
    wstk_inthash_t *table = NULL;

    if(wstk_inthash_init(&table) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_hash_init()");
        return -1;
    }

    WSTK_DBG_PRINT("insert key=10101...");
    wstk_inthash_insert_ex(table, 10101, wstk_str_dup("VAL-1_0"), true);
    WSTK_DBG_PRINT("key1=%s", wstk_core_inthash_find(table, 10101));

    WSTK_DBG_PRINT("replace key=10101...");
    wstk_inthash_insert_ex(table, 10101, wstk_str_dup("VAL-1_1"), true);
    WSTK_DBG_PRINT("key1=%s", wstk_core_inthash_find(table, 10101));

    WSTK_DBG_PRINT("delete key=10101...");
    wstk_core_inthash_delete(table, 10101);
    WSTK_DBG_PRINT("key1=%s", wstk_core_inthash_find(table, 10101));

    WSTK_DBG_PRINT("table size=%d", wstk_hash_size(table));

    // ------------------------------------------------------------------------
    WSTK_DBG_PRINT("insert %d items...", max_items);
    for(int i=0; i < max_items; i++) {
        char *val = NULL;

        wstk_sdprintf(&val, "value__%i", i);

        //wstk_inthash_insert(table, i, val);
        wstk_inthash_insert_ex(table, i, val, true);
    }

    if(!wstk_hash_is_empty(table)) {
        wstk_hash_index_t *hidx = NULL;
        char *val=NULL;
        void *key = NULL;
        uint32_t ikey=0;

        for(hidx = wstk_hash_first_iter(table, hidx); hidx; hidx = wstk_hash_next(&hidx)) {
            wstk_hash_this(hidx, (void *)&key, NULL, (void *)&val);
            ikey = *((uint32_t *)key);

            if(!key || !val) {
                WSTK_DBG_PRINT("corrupted item: key=%d, val=%p, idx=%p", key, val, hidx);
                break;
            }
            //WSTK_DBG_PRINT("key=%d, val=%s", ikey, val);
        }
    }

    wstk_mem_deref(table);
    return 0;
}

void start_example(int argc, char **argv) {
    int err = 0;

    WSTK_DBG_PRINT("Test hastable (wstk-version: %s)", WSTK_VERSION_STR);

    if((err = str_hash(10000))) {
        WSTK_DBG_PRINT("str_hash() err=%d", err);
        return;
    }

    WSTK_DBG_PRINT("--------------------------------------------------");

    if((err = int_hash(10000))) {
        WSTK_DBG_PRINT("int_hash() err=%d", err);
        return;
    }
}
