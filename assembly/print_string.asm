print_string:
	pusha ; Push all registers to stack
	mov ah, 0x0e ; BIOS teletype routine
loop:
	mov cl, [bx] ; Read from address
	cmp cl, 0 ; Check for null terminator
	je break ; If end of string, break loop
	mov al, cl ; Copy char to be printed
	int 0x10 ; Print char
	add bx, 1 ; Increment address
	jmp loop ; Repeat
break:
	popa ; Pop all registers from stack
	ret
