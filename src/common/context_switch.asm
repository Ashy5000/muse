.global active_ctx
.global context_switch

context_switch:
	push %ebx
	push %esi
	push %edi
	push %ebp

	movl (active_ctx),%esi
	movl %esp,(%esi)

	movl 20(%esp),%edi
	movl %edi,(active_ctx)
	movl (%edi),%esp
	movl 4(%edi),%eax
	movl %eax,%cr3

	pop %ebp
	pop %edi
	pop %esi
	pop %ebx

	ret
