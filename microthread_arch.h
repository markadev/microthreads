#ifndef _MICROTHREAD_ARCH_H
#define _MICROTHREAD_ARCH_H

#if defined(_WIN32)
#define CDECL __cdecl
#else
#define CDECL
#endif


struct task;

extern int CDECL __uthread_extend_stack_buf(struct task *t,
	unsigned int min_size);
extern int CDECL __uthread_populate_inital_stack(struct task *t,
	void (*run_func)(void *), void *data);
extern void CDECL __uthread_switch_context(struct task **curr_task,
	void *stack_bottom, struct task *next_task);
extern void CDECL __uthread_main(struct task *t, void (*run_func)(void *),
	void *data);

#endif
