#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "microthreads.h"

#define NUM_TEST_TASKS 100000L
#define NUM_LOOPS 100


static struct task test_tasks[NUM_TEST_TASKS];
static int test_task_counters[NUM_TEST_TASKS];


static void test_task_run(void *data) {
	int *test_data = (int *)data;

	char simulatedNormalStackUse[2048];
	(void)simulatedNormalStackUse;

	while(1) {
		(*test_data)++;
		uthread_yield();
	}
}


static void start_test_tasks(void) {
	long i;

	memset(test_tasks, 0, sizeof(test_tasks));
	memset(test_task_counters, 0, sizeof(test_task_counters));
	for(i=0; i < NUM_TEST_TASKS; i++) {
		uthread_start(&(test_tasks[i]), test_task_run,
			&(test_task_counters[i]));
	}
}


static void run_tasks(void) {
	long i;

	uthread_set_stack_bottom(__builtin_frame_address(0));

	for(i=0; i < NUM_LOOPS; i++) {
		uthread_yield();
	}
}


int main(int argc, char *argv[]) {
	struct timeval tv_before, tv_after;

	uthread_init();

	start_test_tasks();

	gettimeofday(&tv_before, NULL);
	run_tasks();
	gettimeofday(&tv_after, NULL);

#if 0
	long i;
	for(i=0; i < NUM_TEST_TASKS; i++)
		printf("task%d: %d\n", i, test_task_counters[i]);
#endif
	long deltat = ((tv_after.tv_sec * 1000) + (tv_after.tv_usec / 1000)) -
		((tv_before.tv_sec * 1000) + (tv_before.tv_usec / 1000));
	printf("%lu context switches in %ld.%ld seconds\n",
		(NUM_TEST_TASKS + 1) * NUM_LOOPS, deltat / 1000, deltat % 1000);

	return 0;
}
