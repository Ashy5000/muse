addr_packet:
	db 0x10
	db 0x00
	dw 57
	dw KERNEL_OFFSET
	dw 0
	dd 2048
	dd 0

; Read a 50 sector long kernel. Done in 2 reads.
read_kernel:
	mov si, addr_packet
	mov ah, 0x42
	mov dl, [BOOT_DRIVE]
	int 0x13
	ret
