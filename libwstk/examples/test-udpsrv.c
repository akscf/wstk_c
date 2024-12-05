//
// usage: ./runsrv 10.0.0.1 1234
// --------
// echo -n "test" | netcat -u 10.0.4.104 1234
// iperf -u -c 10.0.4.104 -p 1234 -t 60
// iperf -u -c 10.0.4.104 -p 1234 -P 10
// iperf -u -c 10.0.4.104 -p 1234 -P 10 -t 60
//

#include <wstk.h>

static bool globa_break = false;
static void int_handler(int dummy) { globa_break = true; }
static uint32_t maxcons = 0;
static uint32_t connections = 0;
static wstk_mutex_t *mutex;

void udp_handler(wstk_udp_srv_conn_t *conn, wstk_mbuf_t *message) {
    wstk_sockaddr_t *peer=NULL;
    char *msg = NULL;

    wstk_mutex_lock(mutex);
    connections++;
    wstk_mutex_unlock(mutex);

    wstk_udp_srv_conn_peer(conn,  &peer);
    wstk_mbuf_strdup(message, &msg, message->end);    
    if(msg) {
	//wstk_printf("Received from [%J] : [%s] | conns=%u\n", peer, msg, connections);

	//wstk_msleep(1000);
	//wstk_mbuf_printf(message, "ECHO: [%s]-%u\n", msg, connections);
	wstk_mbuf_set_pos(message, 0);
	wstk_udp_srv_write(conn, message);

        //wstk_msleep(10000);
    }

    wstk_mutex_lock(mutex);
    connections--;
    wstk_mutex_unlock(mutex);

    return;
}

int main(int argc, char **argv) {
    wstk_sockaddr_t sa = {0};
    wstk_udp_srv_t *srv = NULL;
    char *host = NULL;
    uint32_t port = 0, stat_tmr=0;
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, int_handler);

    if(wstk_core_init() != WSTK_STATUS_SUCCESS) {
	printf("wstk_core_init() failed\n");
	goto exit;
    }

    if(!argc || argc < 3) {
        WSTK_DBG_PRINT("usage: %s ip port [maxcons]", argv[0]);
	goto exit;
    }

    setbuf(stderr, NULL);
    setbuf(stdout, NULL);

    host = argv[1];
    port = atoi(argv[2]);
    maxcons = argc > 3 ? atoi(argv[3]) : 1024;


    if(wstk_sa_set_str(&sa, host, port) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_sa_set_str()");
        goto exit;
    }
    
    wstk_printf("Starting UDP server (%J)....\n", (wstk_sockaddr_t *)&sa);
    if(wstk_udp_srv_create(&srv, &sa, maxcons, 4096, udp_handler) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_udp_srv_create()");
        goto exit;
    }
    if(wstk_udp_srv_start(srv) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_udp_srv_start()");
        goto exit;
    }

    if(wstk_mutex_create(&mutex) != WSTK_STATUS_SUCCESS) {
        WSTK_DBG_PRINT("FAIL: wstk_mutex_create()");
        goto exit;
    }

    wstk_printf("\n\nServer statred on: %J\nUse ctrl+c to terminate one\n-----------------------------------------------\n", (wstk_sockaddr_t *)&sa);
    
    while(true) {
        if(globa_break) {
            break;
        }
	if(!stat_tmr || stat_tmr <= wstk_time_epoch_now()) {
    	    // WSTK_DBG_PRINT("active connections: %i", connections);
	    stat_tmr = wstk_time_epoch_now() + 5;
	}

        wstk_msleep(1000);
    }


    WSTK_DBG_PRINT("Stopping UDP server...");
    wstk_mem_deref(srv);

exit:
    wstk_core_shutdown();
    exit(0);
}

