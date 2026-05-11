get_memory_info:
	pusha
	mov di, 0x0504
	xor ebx, ebx ; Clear EBX
	xor bp, bp ; BP counts entries
	mov edx, 0x534D4150 ; "SMAP" -> edx
	mov eax, 0xE820 ; Function to get a memory map
	mov dword [es:di + 20], 1 ; Write 1 into memory at table address + 20 to force correct format
	mov ecx, 24 ; Request 24 bytes
	int 0x15 ; Do the thing!
	jc memory_failed
	mov edx, 0x534D4150 ; Some BIOSes set this to zero. Don't do that!
	cmp eax, edx ; BIOS should reset EAX to "SMAP"
	jne memory_failed
	test ebx, ebx ; If EBX is 0, the list is 1 element long. Fail.
	je memory_failed
memory_loop:
	; Repeat for subsequent entries...
	mov eax, 0xE820
	mov dword [es:di + 20], 1
	mov ecx, 24
	int 0x15
	jc memory_finished ; If cary is set, the end of the list has been reached.
	mov edx, 0x534D4150
	jcxz memory_skip ; Skip this entry of ECX is equal to zero (zero length entry)
	cmp cl, 20 ; Is the response length less than or equal to 20 bytes?
	jbe memory_short_entry
	test byte [es:di + 20], 1 ; Should it be ignored?
	je memory_skip
memory_short_entry:
	mov eax, [es:di + 8] ; Get lower 32 bits of length
	or eax, [es:di + 12] ; Or it with the upper 32 bits
	jz memory_skip ; If length is zero, skip the entry
	inc bp ; Increment entry count
	add di, 24 ; Extend buffer
memory_skip:
	test ebx, ebx ; If EBX is zero, we've reached the end of the list
	jne memory_loop ; Otherwise, keep going!
memory_finished:
	mov [0x0500], bp ; Store entry count
	clc ; Clear the carry bit
	popa
	ret
memory_failed:
	popa
	ret
