// These functions are the END of the _init and _fini functions.
.section .init
	// crtend.o's .init section is put here by GCC.
	pop %ebp // Restore previous stack frame
	ret

.section .fini
	// crtend.o's .fini section is put here by GCC.
	pop %ebp // Restore previous stack frame
	ret
