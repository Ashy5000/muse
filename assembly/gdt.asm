; Global Descriptor Table (GDT)
; Defines segments of memory.
; This is a flat model, with two segments, one for code and one for data, which overlap.

; db = define byte
; dw = define word
; dd = define double word

gdt_start:
gdt_null: ; The CPU mandates that we begin with a null descriptor.
	dd 0x0
	dd 0x0

gdt_code: ; Code segment
	dw 0xffff ; Limit (bits 0-15)
	dw 0x0 ; Base (bits 0-15)
	db 0x0 ; Base (bits 16-23)
	db 10011010b ; Flags
	db 11001111b ; Flags, limit (bits 16-19)
	db 0x0 ; Base (bits 24-31)

gdt_data: ; Data segment
	dw 0xffff
	dw 0x0
	db 0x0
	db 10010010b
	db 11001111b
	db 0x0

gdt_end: ; End of GDT

gdt_descriptor:
	dw gdt_end - gdt_start - 1 ; The size of the GDT, subtract 1 for some reason.
	dd gdt_start ; The start of the GDT

CODE_SEG equ gdt_code - gdt_start ; The offset of the code segment descriptor
DATA_SEG equ gdt_data - gdt_start ; The offset of the data segment descriptor
