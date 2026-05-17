.global userspace_runway
.global jump_ring3
.global argc
.global argv

jump_ring3:
	mov $0x23, %ax # Index of user data GDT descriptor, or'd with 3 for ring 3.
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	mov %esi, argv
	mov %edi, argc

	# Create a stack frame that tricks iret into returning into ring 3 inside of the function we want it to
	mov %esp, %eax
	push $0x23
	push $0x200000
	pushf
	push $0x1B
	push userspace_runway
	iret
