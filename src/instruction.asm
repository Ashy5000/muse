.globl get_instruction_pointer

get_instruction_pointer:
	mov (%esp), %ebx
	ret
