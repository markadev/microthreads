#include "microthread_arch.h"
#include "microthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


// When reallocating the stack buffer, the amount of extra space to allocate
#define ALLOC_EXTRA 64


#if defined(__x86_64__) || defined(_M_X64)
// XXX: check for win64
#ifdef _WIN64
#error not supported yet
#endif
extern void __uthread_main_adapter(void);

int __uthread_populate_inital_stack(struct task *t,
	void (*run_func)(void *), void *data)
{
	static const unsigned int initial_stack_size = 9 * sizeof(uint64_t);
	uint64_t *p;
	if(__uthread_extend_stack_buf(t, initial_stack_size) != 0)
		return UTHREAD_OUT_OF_MEMORY;

	p = (uint64_t *)t->stack_buf;
	p[0] = 0;                 /* %rbp */
	p[1] = 0;                 /* %rbx */
	p[2] = (uint64_t)t;       /* %r12 */
	p[3] = (uint64_t)run_func;/* %r13 */
	p[4] = (uint64_t)data;    /* %r14 */
	p[5] = 0;                 /* %r15 */
	p[6] = 0x0000037f00001f80ULL; /* fstcw (4 bytes), mxcsr (4 bytes) */
	p[7] = (uint64_t)&__uthread_main_adapter, /* %rip */
	p[8] = 0; /* alignment value that is left on the stack */
	t->stack_buf_size = initial_stack_size;

	return UTHREAD_OK;
}
#endif


#if defined(__i386__) || defined(__i386) || defined(i386) || defined(_M_IX86)
int CDECL __uthread_populate_inital_stack(struct task *t,
	void (*run_func)(void *), void *data)
{
	static const unsigned int initial_stack_size = 9 * sizeof(uint32_t);
	uint32_t *p;
	if(__uthread_extend_stack_buf(t, initial_stack_size) != 0)
		return UTHREAD_OUT_OF_MEMORY;

	p = (uint32_t *)t->stack_buf;
	p[0] = 0;                 /* %edi */
	p[1] = 0;                 /* %esi */
	p[2] = 0;                 /* %ebx */
	p[3] = 0;                 /* %ebp */
	p[4] = (uint32_t)&__uthread_main, /* %eip */
	p[5] = 0;                 /* fake return address */
	p[6] = (uint32_t)t;       /* first func param */
	p[7] = (uint32_t)run_func;/* second func param */
	p[8] = (uint32_t)data;    /* third func param */
	t->stack_buf_size = initial_stack_size;

	return UTHREAD_OK;
}
#endif


// Allocate more space for the stack buffer
int CDECL __uthread_extend_stack_buf(struct task *t, unsigned int min_size) {
	//printf("extending stack of %p to %u\n", t, min_size);
	free(t->stack_buf);
	t->stack_buf = malloc(min_size + ALLOC_EXTRA);
	if(t->stack_buf != NULL) {
		t->stack_buf_capacity = min_size + ALLOC_EXTRA;
		return 0;
	}

	t->stack_buf_capacity = 0;
	t->stack_buf_size = 0;
	return -1;
}
