.set MAGIC, 0xE85250D6
.set ARCH, 0
.set HEADER_LEN, multiboot_end - multiboot_start
.set CHECKSUM, -(MAGIC + ARCH + HEADER_LEN)

.section .multiboot
.align 8
multiboot_start:
	.long MAGIC
	.long ARCH
	.long HEADER_LEN
	.long CHECKSUM
	# Information request tag
.align 8
	.word 1
	.word 1
	.long 24
	.long 6
	.long 8
	.long 14
	.long 15
.align 8
	# Framebuffer tag
	.word 5
	.word 1
	.long 20
	.long 1920
	.long 1080
	.long 0
# .align 8
	# Keep EFI boot services
	# .word 7
	# .word 0
	# .long 8
.align 8
	# Terminating tag
	.word 0
	.word 0
	.long 8
multiboot_end:

.section .bss
.align 16
stack_bottom:
.skip 0x1000
stack_top:

.section .rodata
.align 16
tss:
 	.long 0
 	.long 0
 	.word 0x10
 	.fill 0x5c, 1, 0
 	.word 0x68

gdt_start:
gdt_null:
	.long 0x0
	.long 0x0

gdt_kernel_code:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b10011010
	.byte 0b11101111
	.byte 0x0

gdt_kernel_data:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b10010010
	.byte 0b11101111
	.byte 0x0

gdt_user_code:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b11111010
	.byte 0b11101111
	.byte 0x0

gdt_user_data:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b11110010
	.byte 0b11101111
	.byte 0x0

gdt_tss:
	.word 0x67
tss_base_0:
	.word 0x0
tss_base_1:
	.byte 0x0
	.byte 0x89
	.byte 0x0
tss_base_2:
	.byte 0x0
	.long 0x0

gdt_end:

gdt_descriptor:
	.word gdt_end - gdt_start - 1
	.long gdt_start
	.long 0

.set CODE_SEG, gdt_kernel_code - gdt_start
.set DATA_SEG, gdt_kernel_data - gdt_start

.section .data
.align 4096
pml4:
.fill 4096, 1, 0
pdpt:
.fill 4096, 1, 0
pd:
.fill 4096, 1, 0
pd_end:

.section .text
.code32

.global _start
.type _start, @function
_start:
	mov $stack_top, %esp
	xor %ecx, %ecx
	push %ecx # Pad to 64 bits
	push %eax
	push %ecx
	push %ebx

	lea tss, %eax
	mov %ax, (tss_base_0)
	shr $16, %eax
	mov %al, (tss_base_1)
	shr $8, %eax
	mov %al, (tss_base_2)
	cli

	lea pdpt, %eax
	or $3, %eax
	mov %eax, (pml4)
	lea pd, %eax
	or $3, %eax
	mov %eax, (pdpt)

	xor $3, %eax
	mov $0x83, %ebx
	lea pd_end, %ecx
pg_loop:
	mov %ebx, (%eax)
	add $0x200000, %ebx
	add $8, %eax
	cmp %ecx, %eax
	jne pg_loop

	# PAE
	mov %cr4, %eax
	or $0x20, %eax
	mov %eax, %cr4

	# LME
	mov $0xc0000080, %ecx
	rdmsr
	or $0x100, %eax
	wrmsr

	lea pml4, %eax
	mov %eax, %cr3

	# Enable paging
	mov %cr0, %eax
	or $0x80000001, %eax
	mov %eax, %cr0

	lgdt (gdt_descriptor)
	mov $0x10, %eax
	mov %eax, %ds
	mov %eax, %es
	mov %eax, %fs
	mov %eax, %gs
	mov %eax, %ss
	jmp $0x8,$long_mode

.code64
long_mode:
	pop %rdi
	pop %rsi
	call trampoline_main

.size _start, . - _start
