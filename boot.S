.set ALIGN, 1
.set MEMINFO, 2
.set FLAGS, ALIGN | MEMINFO
.set MAGIC, 0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
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
	.byte 0b11001111
	.byte 0x0

gdt_kernel_data:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b10010010
	.byte 0b11001111
	.byte 0x0

gdt_user_code:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b11111010
	.byte 0b11001111
	.byte 0x0

gdt_user_data:
	.word 0xffff
	.word 0x0
	.byte 0x0
	.byte 0b11110010
	.byte 0b11001111
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

gdt_end:

gdt_descriptor:
	.word gdt_end - gdt_start - 1
	.long gdt_start

.set CODE_SEG, gdt_kernel_code - gdt_start
.set DATA_SEG, gdt_kernel_data - gdt_start

.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %esp
	push %eax
	push %ebx
	cli
	lea tss, %eax
	mov %ax, [tss_base_0]
	shr $16, %eax
	mov %al, [tss_base_1]
	shr $8, %eax
	mov %al, [tss_base_2]
	lgdt [gdt_descriptor]
	call main

.size _start, . - _start
