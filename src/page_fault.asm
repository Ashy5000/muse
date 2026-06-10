.globl process_page_fault
.globl handle_page_fault

handle_page_fault:
	pushal
	cld
	call process_page_fault
	popal
	iret
