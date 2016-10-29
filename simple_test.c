#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "microthreads.h"


static int task1_var;
void task1_run(void *data) {
	int guard = 1111;
	while(1) {
		printf("task1 running\n");
		task1_var++;
		sleep(1);
		uthread_yield();
		assert(guard == 1111);
	}
}

static int task2_var;
void task2_run(void *data) {
	int guard = 2222;
	while(1) {
		printf("task2 running\n");
		task2_var++;
		sleep(1);
		uthread_yield();
		assert(guard == 2222);
	}
}

static struct task task1;
static struct task task2;


static void run_tasks(void) {
	uthread_set_stack_bottom(__builtin_frame_address(0));

	int i;
	for(i=0; i < 5; i++) {
		uthread_yield();
		printf("main thread running\n");
		sleep(1);
	}
}


int main(int argc, char *argv[]) {
	uthread_init();

	printf("task1=%p, task2=%p\n", (void *)&task1, (void *)&task2);
	uthread_start(&task1, task1_run, (void *)1234);
	uthread_start(&task2, task2_run, (void *)2345);

	run_tasks();
	printf("task1_var=%d, task2_var=%d\n", task1_var, task2_var);

	return 0;
}
