// These functions are the START of the _init and _fini functions.

.section .init
.global _init
_init:
	// Initialize the stack frame
	push %ebp
	mov %esp, %ebp
	// GCC puts crtbegin.o's .init section here.

.section .fini
.global _fini
_fini:
	// Initialize the stack frame
	push %ebp
	mov %esp, %ebp
	// GCC puts crtbegin.o's .fini section here.
