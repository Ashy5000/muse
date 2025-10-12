; Read a DH sector long kernel.
read_kernel:
	mov ah, 0x02 ; BIOS read sector function
	mov al, dh ; Read DH sectors
	mov ch, 0x00 ; Cylinder 0
	mov dh, 0x00 ; Head 0
	mov cl, 0x02 ; Second sector (because boot sector should be skipped)
	int 0x13 ; Trigger BIOS interrupt
	ret
