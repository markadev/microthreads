#ifndef _MICROTHREADS_H
#define _MICROTHREADS_H

#ifdef __cplusplus
extern "C" {
#endif


#define UTHREAD_OK             0
#define UTHREAD_CANCELLED      1
#define UTHREAD_OUT_OF_MEMORY  2


struct task_list_node {
	struct task_list_node *next;
	struct task_list_node *prev;
};

struct task_list {
	struct task_list_node anchor;
};


// Note: do not change the offsets of the stack members without also changing
// the offset constants in the assembly code
struct task { // TODO rename this struct
	void *stack_buf;
	unsigned int stack_buf_capacity;
	unsigned int stack_buf_size;
	unsigned int flags;

	struct task_list_node ll;

	unsigned long long alarm_time;
	long alarm_heap_idx;
};


struct uthread_cond {
	unsigned int magic;

	struct task_list wait_queue;
};


/**
 * Initializes the microthread library.
 */
extern int uthread_init(void);

/**
 * Performs an orderly shutdown of the microthread library. Microthreads
 * that are still running will be cancelled.
 */
extern void uthread_shutdown(void);

// TODO
extern void uthread_set_stack_bottom(void *ptr);


/**
 * Creates a new microthread and schedules it for execution.
 */
extern int uthread_start(struct task *t, void (*run_func)(void *), void *data);

/**
 * Sets a microthread's cancellation flag.
 */
extern void uthread_cancel(struct task *t);

/**
 * Checks the cancellation flag for the current microthread. A value of
 * 1 is returned if the flag is set, 0 if it is not set.
 */
extern int uthread_is_cancelled(void);

/**
 * Yields execution to the next runnable microthread.
 *
 * On return, the microthread's cancellation flag may have been set.
 */
extern int uthread_yield(void);

/**
 * Suspends execution of the current microthread for at least the
 * requested amount of time.
 *
 * On return, the microthread's cancellation flag may have been set.
 */
extern int uthread_sleep(unsigned int ms);

/**
 * Switches execution to a specific microthread.
 */
extern int uthread_switch_to(struct task *t);


/**
 * Initializes a condition variable for use.
 */
extern void uthread_cond_init(struct uthread_cond *cond);

/**
 * Destroys a condition variable. Any microthreads that are still waiting
 * on the condition will be cancelled.
 */
extern void uthread_cond_destroy(struct uthread_cond *cond);

/**
 * Waits for a condition variable to be signalled either with
 * uthread_cond_broadcast() or uthread_cond_signal().
 *
 * On return, the microthread's cancellation flag may have been set.
 */
extern int uthread_cond_wait(struct uthread_cond *cond);

/**
 * Waits a fixed amount of time for a condition variable to be signalled.
 *
 * On return, the microthread's cancellation flag may have been set.
 */
extern int uthread_cond_timedwait(struct uthread_cond *cond, unsigned int ms);

/**
 * Wakes up a single microthread that is waiting on a condition variable.
 */
extern void uthread_cond_signal(struct uthread_cond *cond);

/**
 * Wakes up all microthreads that are waiting on a condition variable.
 */
extern void uthread_cond_broadcast(struct uthread_cond *cond);

#ifdef __cplusplus
}
#endif

#endif
