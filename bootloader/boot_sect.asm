[org 0x7c00]

KERNEL_OFFSET equ 0x8000

mov [BOOT_DRIVE], dl ; BIOS stores boot drive in dl. Store into memory.

mov ebp, 0x9fbff ; Move stack to top of memory
mov esp, ebp

in al, 0x92
or al, 2
out 0x92, al

call get_memory_info

call read_kernel ; Read the kernel from the disk

call protected_ascend ; The journey begins...
jmp $

BOOT_DRIVE:
	db 0

%include "print_string.asm"
%include "memory.asm"
%include "disk.asm"
%include "gdt.asm"
%include "32protected.asm"

times 406-($-$$) db 0 ; Pad with zeroes

%include "tss.asm"

dw 0xaa55 ; Add a magic number at the end of the file, bringing the length to 512 bytes
