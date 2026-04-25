[bits 16]

protected_ascend:
	mov ax, 0
	mov ss, ax
	mov sp, 0xFFFC
	mov ax, 0 ; Clear segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	cli ; Disable interrupts
	lgdt [gdt_descriptor] ; Load the GDT descriptor
	mov eax, cr0
	or eax, 0x1
	mov cr0, eax ; Set leftmost bit of control register 0 to make the switch
	jmp CODE_SEG:protected_init
	jmp $
[bits 32] ; From now on, we are in 32 bit protected mode

VIDEO_MEMORY equ 0xb8000

protected_init:
	mov ax, DATA_SEG ; Set up segment registers to point to the data segment
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	mov ebp, 0x9fbff ; Move stack to top of memory
	mov esp, ebp

	mov ax, 0x28 ; Load the TSS
	ltr ax

	call kernel_ascend
	jmp $

kernel_ascend:
	call KERNEL_OFFSET ; To the kernel!
	jmp $

[bits 16]
