#define OFFSETOF_STACKBUF 0
#define OFFSETOF_STACKBUF_CAPACITY 4
#define OFFSETOF_STACKBUF_SIZE 8

.altmacro

# Parameters:
#   %ecx - number of bytes to copy
#   %esi - copy source. Must be aligned to 4 bytes
#   %edi - copy destination. Must be aligned to 4 bytes
#
# Clobbers:
#   %ecx, %edx, %esi, %edi
.macro fast_stack_copy
LOCAL copy1
	mov %ecx, %edx
	and $0x3, %edx
	shr $2, %ecx
	jz copy1
	rep movsl

copy1:
	mov %edx, %ecx
	rep movsb
.endm


.text


# Once the stack frame is set up, the function parameters are:
#   8(%ebp)  -> struct task **curr_task
#   12(%ebp) -> void *stack_bottom
#   16(%ebp) -> struct task *next_task
.global __uthread_switch_context
.align 16
__uthread_switch_context:
	# Save the registers that must be preserved over function calls
	push %ebp
	mov %esp, %ebp
	push %ebx
	push %esi
	push %edi


	# Grab a pointer to the current task
	mov 8(%ebp), %ebx
	mov (%ebx), %ebx

	# Check if the current stackbuf capacity is sufficient
	mov OFFSETOF_STACKBUF_CAPACITY(%ebx), %eax
	mov 12(%ebp), %ecx
	sub %esp, %ecx
	cmp %ecx, %eax
	jl 0f
1:
	mov %ecx, OFFSETOF_STACKBUF_SIZE(%ebx)

	# Copy out the stack of the current microthread
	mov OFFSETOF_STACKBUF(%ebx), %edi
	mov %esp, %esi
	fast_stack_copy

	# Set curr_task to the new task
	mov 16(%ebp), %ebx
	mov 8(%ebp), %eax
	mov %ebx, (%eax)

	# Copy in the stack of the next thread
	# memcpy(stack_bottom - next_stack_size, next_stack_top, next_stack_size);
	mov OFFSETOF_STACKBUF_SIZE(%ebx), %ecx
	mov OFFSETOF_STACKBUF(%ebx), %esi
	mov 12(%ebp), %edi
	sub %ecx, %edi
	mov %edi, %esp   # set new stack pointer here
	fast_stack_copy

	# Restore the preserved registers from the new stack
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret

0:
	# Need to make the stack buf larger
	push %ecx   # min size
	push %ebx   # task
	call __uthread_extend_stack_buf
	# TODO: check return code
	pop %ebx
	pop %ecx # IMPORTANT to restore
	jmp 1b
