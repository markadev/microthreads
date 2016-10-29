#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "microthreads.h"


static int test_var;
static struct uthread_cond test_var_cond;

static int test2_work_var;
static int test3_work_var;


void task1_run(void *data) {
	while(1) {
		printf("task1: doing work\n");
		test_var++;
		uthread_cond_broadcast(&test_var_cond);

		printf("task1: work done\n\n");
		uthread_yield();
	}
}


void task2_run(void *data) {
	while(1) {
		printf("task2: waiting for ((test_var %% 4) == 3): %d\n", test_var);
		while((test_var % 4) != 3) {
			printf("task2: condition not satified - waiting\n\n");
			uthread_cond_wait(&test_var_cond);
			printf("task2: awoken\n");
		}
		printf("task2: condition on test_var satisfied: %d\n", test_var);

		printf("task2: doing work\n");
		test2_work_var++;

		printf("task2: work done\n\n");
		uthread_yield();
	}
}


void task3_run(void *data) {
	while(1) {
		printf("task3: waiting for ((test_var %% 2) == 1): %d\n", test_var);
		while((test_var % 2) != 1) {
			printf("task3: condition not satified - waiting\n\n");
			uthread_cond_wait(&test_var_cond);
			printf("task3: awoken\n");
		}
		printf("task3: condition on test_var satisfied: %d\n", test_var);

		printf("task3: doing work\n");
		test3_work_var++;

		printf("task3: work done\n\n");
		uthread_yield();
	}
}


#if 0
// This will naturally spend most of its time waiting on the condition
// that gets signalled the least frequently
void multi_cond_wait(void) {
	// Need foo == 4 and bar == 1

	int satisfied;
	do {
		satisfied = 1;
		if(foo != 4) {
			satisfied = 0;
			uthread_cond_wait(&foo_cond);
		}

		if(bar != 1) {
			satified = 0;
			uthread_cond_wait(&bar_cond);
		}
	} while(! satisified);
}
#endif


struct task task1;
struct task task2;
struct task task3;


static void run_tasks(void) {
	uthread_set_stack_bottom(__builtin_frame_address(0));

	while(test2_work_var < 3) {
		printf("main: doing work\n");
		printf("main: work done\n\n");
		uthread_yield();
	}
}


int main(int argc, char *argv[]) {
	uthread_init();

	printf("task1=%p, task2=%p, task3=%p\n", (void *)&task1,
		(void *)&task2, (void *)&task3);
	uthread_cond_init(&test_var_cond);
	uthread_start(&task1, task1_run, (void *)1234);
	uthread_start(&task2, task2_run, (void *)2345);
	uthread_start(&task3, task3_run, (void *)3456);

	run_tasks();

	assert(test_var == 12);
	assert(test2_work_var == 3);
	assert(test3_work_var == 6);
	printf("OK\n");

	return 0;
}
