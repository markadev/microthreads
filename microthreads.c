#include "microthreads.h"
#include "microthread_arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> // for offsetof()
#include <stdint.h>
#include <string.h>
#include <assert.h>


#define UTHREAD_STATE_MASK     0x00000003
#define UTHREAD_FLAG_CANCELLED 0x00000008
#define UTHREAD_FLAG_ALARM     0x00000010

/* The states a microthread can be in:
 *   RUNNABLE - It is either running (it is current_task) or it can be run
 *              (it is in run_queue).
 *   WAITING  - It is waiting for some event to occur. It may or may not be
 *              in some object's wait queue.
 *   ZOMBIE   - The main routine of the microthread has returned so it will
 *              never execute again. It will be in the zombie_list until
 *              it has been reaped.
 */
#define UTHREAD_STATE_RUNNABLE        0x0
#define UTHREAD_STATE_WAITING         0x1
#define UTHREAD_STATE_ZOMBIE          0x3


#define UTHREAD_COND_MAGIC 0x436F6E64


static int initialized;
static void *stack_bottom_addr;

static struct task main_task;
static struct task *current_task;

// A task is only in this list when it can be run but is not currently running
static struct task_list run_queue;

// A list of tasks that have ended and must be destroyed.
static struct task_list zombie_list;



static void reschedule_task(struct task *t);
static void switch_to_next_task(void);



static inline void task_list_init(struct task_list *l) {
	l->anchor.next = &(l->anchor);
	l->anchor.prev = &(l->anchor);
}

static inline int task_list_empty(struct task_list *l) {
	if(l->anchor.next == &(l->anchor))
		return 1;
	return 0;
}

static inline struct task *task_list_remove_head(struct task_list *l) {
	struct task_list_node *n = l->anchor.next;
	if(n != &(l->anchor)) {
		l->anchor.next = n->next;
		n->next->prev = &(l->anchor);
		n->next = NULL;
		n->prev = NULL;

		/* Convert the pointer from pointing to task->ll to task */
		return (struct task *)(((char *)n) - offsetof(struct task, ll));
	}
	return NULL;
}

static inline void task_list_append_tail(struct task_list *l, struct task *t) {
	struct task_list_node *n = &(t->ll);
	assert(n->next == NULL);
	assert(n->prev == NULL);

	n->next = &(l->anchor);
	n->prev = l->anchor.prev;
	l->anchor.prev->next = n;
	l->anchor.prev = n;
}

static inline void task_list_remove(struct task_list_node *n) {
	assert(n->next != NULL);
	assert(n->prev != NULL);
	n->prev->next = n->next;
	n->next->prev = n->prev;
	n->next = NULL;
	n->prev = NULL;
}


static inline void set_task_state(struct task *t, unsigned int state) {
	t->flags = (t->flags & ~UTHREAD_STATE_MASK) | state;
}

static inline unsigned int get_task_state(const struct task *t) {
	return (t->flags & UTHREAD_STATE_MASK);
}



int uthread_init(void) {
	assert(initialized == 0);

	memset(&main_task, 0, sizeof(struct task));
	main_task.alarm_heap_idx = -1;
	set_task_state(&main_task, UTHREAD_STATE_RUNNABLE);
	current_task = &main_task;
	task_list_init(&run_queue);
	task_list_init(&zombie_list);
	initialized = 1;

	return UTHREAD_OK;
}


void uthread_shutdown(void) {
	assert(initialized == 1);

	// TODO: cancel microthreads still running. we need a list of
	// all microthreads.

	assert(task_list_empty(&run_queue));
	assert(task_list_empty(&zombie_list));
	current_task = NULL;
	free(main_task.stack_buf);
	memset(&main_task, 0, sizeof(struct task));
	stack_bottom_addr = NULL;
	initialized = 0;
}


// TODO: take a range of addresses
void uthread_set_stack_bottom(void *ptr) {
	assert(initialized == 1);

	if(stack_bottom_addr != NULL) {
		if(stack_bottom_addr != ptr) {
			fprintf(stderr, "stack_bottom_addr changed!\n");
			abort();
		}
	} else {
		stack_bottom_addr = ptr;
	}
}


int uthread_start(struct task *t, void (*run_func)(void *), void *data) {
	assert(t != NULL);
	assert(run_func != NULL);
	assert(initialized == 1);

	memset(t, 0, sizeof(struct task));
	set_task_state(t, UTHREAD_STATE_RUNNABLE);
	t->alarm_heap_idx = -1;

	int rc = __uthread_populate_inital_stack(t, run_func, data);
	if(rc != UTHREAD_OK)
		return rc;
	reschedule_task(t);

	return UTHREAD_OK;
}


void uthread_cancel(struct task *t) {
	assert(t != NULL);
	assert(initialized == 1);

	switch(get_task_state(t)) {
	case UTHREAD_STATE_WAITING:
		t->flags |= UTHREAD_FLAG_CANCELLED;
		// Wake the microthread from its slumber
		task_list_remove(&(t->ll));
		reschedule_task(t);
		break;
	case UTHREAD_STATE_RUNNABLE:
		t->flags |= UTHREAD_FLAG_CANCELLED;
		// Microthread is already "awake"
		break;
	default:;
	}
}


int uthread_is_cancelled(void) {
	assert(initialized == 1);

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return 1;
	return 0;
}


int uthread_yield(void) {
	assert(initialized == 1);
	assert(get_task_state(current_task) == UTHREAD_STATE_RUNNABLE);

	reschedule_task(current_task);
	switch_to_next_task();

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return UTHREAD_CANCELLED;
	return UTHREAD_OK;
}


#if 0
int uthread_sleep(unsigned int ms) {
	assert(initialized == 1);
	assert(get_task_state(current_task) == UTHREAD_STATE_RUNNABLE);
	assert(current_task->alarm_heap_idx == -1);

	/* Clear the alarm clock flag */
	current_task->flags &= ~UTHREAD_FLAG_ALARM;

	// TODO: add an entry to the alarm heap

	/* Wait for the alarm clock to go off (or cancellation) */
	set_task_state(current_task, UTHREAD_STATE_WAITING);
	switch_to_next_task();

	// TODO make sure our alarm heap entry is no longer there

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return UTHREAD_CANCELLED;
	return UTHREAD_OK;
}
#endif


int uthread_switch_to(struct task *t) {
	assert(initialized == 1);

	if((t != current_task) && (get_task_state(t) != UTHREAD_STATE_ZOMBIE)) {
		/* The microthread is either on the run queue or in a wait queue.
		 * Remove it and switch to it. */
		task_list_remove(&(t->ll));
		set_task_state(t, UTHREAD_STATE_RUNNABLE);
		__uthread_switch_context(&current_task, stack_bottom_addr, t);
	}

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return UTHREAD_CANCELLED;
	return UTHREAD_OK;
}


void uthread_cond_init(struct uthread_cond *cond) {
	assert(cond != NULL);

	memset(cond, 0, sizeof(struct uthread_cond));
	cond->magic = UTHREAD_COND_MAGIC;
	task_list_init(&(cond->wait_queue));
}


void uthread_cond_destroy(struct uthread_cond *cond) {
	assert(cond != NULL);
	assert(cond->magic == UTHREAD_COND_MAGIC);

	if((cond != NULL) && (cond->magic == UTHREAD_COND_MAGIC)) {
		assert((initialized == 1) || task_list_empty(&(cond->wait_queue)));

		while(! task_list_empty(&(cond->wait_queue))) {
			struct task *t = task_list_remove_head(&(cond->wait_queue));
			t->flags |= UTHREAD_FLAG_CANCELLED;
			reschedule_task(t);
		}
		cond->magic = 0;
	}
}


int uthread_cond_wait(struct uthread_cond *cond) {
	assert(initialized == 1);
	assert(cond != NULL);
	assert(cond->magic == UTHREAD_COND_MAGIC);
	assert(get_task_state(current_task) == UTHREAD_STATE_RUNNABLE);

	/* Add ourself to the queue of waiting tasks */
	set_task_state(current_task, UTHREAD_STATE_WAITING);
	task_list_append_tail(&(cond->wait_queue), current_task);

	/* Other microthreads run while we wait... */
	switch_to_next_task();

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return UTHREAD_CANCELLED;
	return UTHREAD_OK;
}


#if 0
// Waits a fixed amount of time for a condition variable to be signalled.
int uthread_cond_timedwait(struct uthread_cond *cond, unsigned int ms) {
	assert(initialized == 1);
	assert(cond != NULL);
	assert(cond->magic == UTHREAD_COND_MAGIC);
	assert(get_task_state(current_task) == UTHREAD_STATE_RUNNABLE);
	assert(current_task->alarm_heap_idx == -1);

	// Add ourself to the queue of waiting tasks
	set_task_state(current_task, UTHREAD_STATE_WAITING);
	task_list_append_tail(&(cond->wait_queue), current_task);

	// TODO: set the alarm

	// Other microthreads run while we wait...
	switch_to_next_task();

	if(current_task->flags & UTHREAD_FLAG_CANCELLED)
		return UTHREAD_CANCELLED;
	if(current_task->flags & UTHREAD_FLAG_ALARM)
		return UTHREAD_TIMEOUT;
	return UTHREAD_OK;
}
#endif


void uthread_cond_signal(struct uthread_cond *cond) {
	struct task *t;

	assert(initialized == 1);
	assert(cond != NULL);
	assert(cond->magic == UTHREAD_COND_MAGIC);

	/* Wake the first microthread in the wait queue */
	t = task_list_remove_head(&(cond->wait_queue));
	if(t != NULL) {
		assert(get_task_state(t) == UTHREAD_STATE_WAITING);
		reschedule_task(t);
	}
}


void uthread_cond_broadcast(struct uthread_cond *cond) {
	struct task *t;

	assert(initialized == 1);
	assert(cond != NULL);
	assert(cond->magic == UTHREAD_COND_MAGIC);

	/* Wake all microthreads in the wait queue */
	while(! task_list_empty(&(cond->wait_queue))) {
		t = task_list_remove_head(&(cond->wait_queue));
		assert(get_task_state(t) == UTHREAD_STATE_WAITING);
		reschedule_task(t);
	}
}


// Reschedules a task for execution. The task is added to the run queue
// and its state is set to RUNNABLE. It must not be on any other task list.
static void reschedule_task(struct task *t) {
	assert((t->ll.next == NULL) && (t->ll.prev == NULL));
	assert(get_task_state(t) != UTHREAD_STATE_ZOMBIE);

	set_task_state(t, UTHREAD_STATE_RUNNABLE);
	task_list_append_tail(&run_queue, t);
}


// Determine the next microthread to run and switch to it.
static void switch_to_next_task(void) {
	// Get the next microthread to run
	struct task *t = task_list_remove_head(&run_queue);
	assert(t != NULL); // XXX: assume at least 1 task always runnable

	if(t != current_task) {
		assert(get_task_state(t) == UTHREAD_STATE_RUNNABLE);
		__uthread_switch_context(&current_task, stack_bottom_addr, t);
	}
}


// The main entry point of a microthread. This is called from the
// architecture-dependent code.
void CDECL __uthread_main(struct task *t, void (*run_func)(void *), void *data)
{
	(*run_func)(data);

	/* Turn into a zombie */
	set_task_state(t, UTHREAD_STATE_ZOMBIE);
	task_list_append_tail(&zombie_list, t);
	// TODO: uthread_cond_signal(&zombie_cond);

	/* Zombie need braaaaaains! */
	switch_to_next_task();
	abort();   /* Should not get here */
}
