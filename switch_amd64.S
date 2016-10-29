#define OFFSETOF_STACKBUF 0
#define OFFSETOF_STACKBUF_CAPACITY 8
#define OFFSETOF_STACKBUF_SIZE 12

.altmacro

# Parameters:
#   %ecx - number of bytes to copy
#   %rsi - copy source. Must be aligned to 8 bytes
#   %rdi - copy destination. Must be aligned to 8 bytes
#
# Clobbers:
#   %rcx, %rdx, %rsi, %rdi, %r8, %r9, %r10, %r11
.macro fast_stack_copy
LOCAL copy32_loop, copy8, copy8_loop, copy1
	mov %ecx, %edx
	and $0x1f, %edx
	shr $5, %ecx
	jz copy8
copy32_loop:
	dec %ecx
	mov     (%rsi), %r8
	mov 0x8 (%rsi), %r9
	mov 0x10(%rsi), %r10
	mov 0x18(%rsi), %r11
	mov %r8,     (%rdi)
	mov %r9,  0x8(%rdi)
	mov %r10, 0x10(%rdi)
	mov %r11, 0x18(%rdi)
	lea 0x20(%rsi), %rsi
	lea 0x20(%rdi), %rdi
	jnz copy32_loop

copy8:
	mov %edx, %ecx
	and $0x07, %edx
	shr $3, %ecx
	jz copy1
copy8_loop:
	dec %ecx
	mov (%rsi), %r8
	mov %r8, (%rdi)
	lea 0x8(%rsi), %rsi
	lea 0x8(%rdi), %rdi
	jnz copy8_loop

copy1:
	mov %edx, %ecx
	rep movsb
.endm


.text

# We MUST not clobber r12-r15, rbx, rbp, the x87 control word, and mxcsr.
# Parameters are passed in %rdi, %rsi, %rdx, %rcx, %r8, %r9
.global __uthread_switch_context
.align 16
__uthread_switch_context:
	# Save the registers that must be preserved over function calls. This
	# also aligns the stack to 16 bytes which is required to make function
	# calls into C code.
	mov %rbp, -0x38(%rsp)
	mov %rbx, -0x30(%rsp)
	mov %r12, -0x28(%rsp)
	mov %r13, -0x20(%rsp)
	mov %r14, -0x18(%rsp)
	mov %r15, -0x10(%rsp)
	stmxcsr   -0x08(%rsp)
	fstcw     -0x04(%rsp)
	sub $0x38, %rsp

	# Grab a pointer to the current task
	mov (%rdi), %rbp

	# Save our function parameters
	mov %rdi, %rbx # struct task **curr_task
	mov %rsi, %r12 # void *stack_bottom
	mov %rdx, %r13 # struct task *next_task

	# Check if the current stackbuf capacity is sufficient
	mov OFFSETOF_STACKBUF_CAPACITY(%rbp), %eax
	mov %r12, %rcx
	sub %rsp, %rcx
	cmp %rcx, %rax
	jl 0f

1:
	mov %ecx, OFFSETOF_STACKBUF_SIZE(%rbp)

	# Copy out the stack of the current thread
	mov OFFSETOF_STACKBUF(%rbp), %rdi
	mov %rsp, %rsi
	fast_stack_copy

	# Copy in the stack of the next thread
	# memcpy(stack_bottom - next_stack_size, next_stack_top, next_stack_size);
	mov OFFSETOF_STACKBUF_SIZE(%r13), %ecx
	mov OFFSETOF_STACKBUF(%r13), %rsi
	mov %r12, %rdi
	sub %rcx, %rdi
	mov %rdi, %rsp   # set new stack pointer here
	fast_stack_copy

	# Set curr_task to the new task
	mov %r13, (%rbx)

	# Restore the preserved registers from the new stack
	add $0x38, %rsp
	mov -0x38(%rsp), %rbp
	mov -0x30(%rsp), %rbx
	mov -0x28(%rsp), %r12
	mov -0x20(%rsp), %r13
	mov -0x18(%rsp), %r14
	mov -0x10(%rsp), %r15
	ldmxcsr -0x08(%rsp)
	fldcw   -0x04(%rsp)
	ret

0:
	# Need to make the stack buf larger
	mov %rcx, %r15
	mov %rbp, %rdi
	mov %rcx, %rsi
	call __uthread_extend_stack_buf
	# TODO: check return code
	mov %r15, %rcx
	jmp 1b



# This function is just an adapter that will take the function parameters
# from the restored registers and pass them in the appropriate C parameter
# registers. Parameters are passed in %r12-%r15
#
# This function is only used the first time a microthread is started.
.global __uthread_main_adapter
__uthread_main_adapter:
	mov %r12, %rdi
	mov %r13, %rsi
	mov %r14, %rdx
	mov %r15, %rcx
	jmp __uthread_main
