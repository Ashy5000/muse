.section .text
.global _start
_start:
	// First node in the stack frame linked list.
	xor %ebp, %ebp
	push %ebp // eip = 0 (no function to return to)
	push %ebp // ebp = 0 (no stack frame under this)
	mov %esp, %ebp

	push %esi // argv
	push %edi // argc

	// TODO: Init std
	call _init // Global constructors

	pop %edi // argc
	pop %esi // argv

	call main // Start of program
	
	mov %eax, %edi // Store exit code
	call exit
	// TODO: Exit

.size _start, . - _start // Store size of _start symbol
