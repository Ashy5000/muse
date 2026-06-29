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
 	.long 0
 	.long 0
 	.long 32
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

.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %rsp
	pushq %rax
	pushq %rbx
	call trampoline_main

.size _start, . - _start
