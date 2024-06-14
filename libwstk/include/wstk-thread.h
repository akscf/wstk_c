/**
 **
 ** (C)2019 aks
 **/
#ifndef WSTK_THREAD_H
#define WSTK_THREAD_H
#include <wstk-core.h>
#include <wstk-sleep.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WSTK_SCHED_YIELD(t) { wstk_thread_yield(); wstk_msleep(t > 0 ? t : 250); }

typedef struct wstk_thread_s wstk_thread_t;
/**
 * arg1 - a thread control struct
 * arg2 - a user specified data
 **/
typedef void (*wstk_thread_func_t)(wstk_thread_t *, void *);


/**
 ** Gives control to a sysrtem shceduler
 ** maintly this uses a thre-sleep(0)
 ** can be used in thread coupled with wstk_msleep()
 **
 **/
wstk_status_t wstk_thread_yield();

/**
 * Create a new thread
 * If you gonna use a control structure don't forget to do 'wstk_mem_deref()' in the end!
 *
 * @param thread - NULL or a new thread control structure
 * @param func   - a user function that will be called
 * @param udata  - a some user data
 * @param uflags - a some user flags
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_create(wstk_thread_t **thread, wstk_thread_func_t func, void *udata, uint32_t uflags);

/**
 * Create a new thread
 * If you gonna use a control structure don't forget to do 'wstk_mem_deref()' in the end!
 *
 * @param thread     - NULL or a new thread control structure
 * @param func       - a user function that will be called
 * @param stack_size - with specified stack size
 * @param udata      - a some user data
 * @param uflags     - a some user flags
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_create2(wstk_thread_t **thread, wstk_thread_func_t func, uint32_t stack_size, void *udata, uint32_t uflags);

/**
 * Jon to the thread
 *
 * @param th - a thread control structure
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_join(wstk_thread_t *th);

/**
 * Make an attempt to (softly) interrupt the thread
 *
 * @param th        - a thread control structure
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_try_cancel(wstk_thread_t *th);

/**
 * Get a thread-id from the thread control structure
 *
 * @param th        - a thread control structure
 * @param id        - a thread id
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_id(wstk_thread_t *th, uint32_t *id);

/**
 * Get/Set a user specified flsgs
 *
 * @param th        - a thread control structure
 * @param flags     - the flags
 *
 * @return sucesss or some error
 **/
wstk_status_t wstk_thread_uflags(wstk_thread_t *th, uint32_t *flags);
wstk_status_t wstk_thread_set_uflags(wstk_thread_t *th, uint32_t flags);

/**
 * Check on a soft interruption
 *
 * @param th        - a thread control structure
 *
 * @return true/false
 **/
bool wstk_thread_is_canceled(wstk_thread_t *th);

/**
 * When the user function finished
 *
 * @param th        - a thread control structure
 *
 * @return true/false
 **/
bool wstk_thread_is_finished(wstk_thread_t *th);



#ifdef __cplusplus
}
#endif
#endif
