[org 0x7c00]

KERNEL_OFFSET equ 0x1000

mov bp, 0x9000 ; Bottom of the stack (higher address)
mov sp, bp ; Top of the stack (lower address)

mov [BOOT_DRIVE], dl ; BIOS stores boot drive in dl. Store into memory.

mov ax, 2403h
int 15h

mov bx, KERNEL_OFFSET ; Address in memory to load to
mov dh, 50 ; Load 50 sectors
mov dl, [BOOT_DRIVE] ; Load from boot drive
call read_kernel ; Read the kernel from the disk

call protected_ascend ; The journey begins...
jmp $

BOOT_DRIVE:
	db 0

%include "print_string.asm"
%include "disk.asm"
%include "gdt.asm"
%include "32protected.asm"

times 510-($-$$) db 0 ; Pad boot sector to 510 bytes with zeroes

dw 0xaa55 ; Add a magic number at the end of the file, bringing the length to 512 bytes
