.global syscall_isr
.global handle_syscall

syscall_isr:
	push %gs
	push %fs
	push %es
	push %ds
	push %edx
	push %ecx
	push %ebx
	push %eax
	push %esp
	call handle_syscall
	add $4, %esp
	pop %eax
	pop %ebx
	pop %ecx
	pop %edx
	pop %ds
	pop %es
	pop %fs
	pop %gs
	iret
