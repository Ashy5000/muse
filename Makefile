C_SOURCES = $(wildcard kernel/*.c drivers/*.c)
ASM_SOURCES = $(wildcard kernel/*.asm drivers/*.asm)
OBJS = ${C_SOURCES:.c=.o} ${ASM_SOURCES:.asm=.o}

boot_sect.bin: assembly/
	cd assembly; nasm boot_sect.asm -f bin -o ../boot_sect.bin

%.o: %.c
	$(HOME)/opt/cross/bin/i686-elf-gcc -ffreestanding -c $< -o $@ -Os -g

%.o: %.asm
	$(HOME)/opt/cross/bin/i686-elf-as $< -o $@ -g

kernel.bin: kernel_entry.o ${OBJS}
	$(HOME)/opt/cross/bin/i686-elf-ld -o $@ -Ttext 0x1000 $^ $(HOME)/opt/cross/lib/gcc/x86_64-elf/15.2.0/libgcc.a --oformat binary --entry main -g

disk.bin: boot_sect.bin kernel.bin
	cat boot_sect.bin kernel.bin > disk.bin

build: disk.bin

run: build
	qemu-system-i386 -drive format=raw,file=disk.bin -no-reboot -no-shutdown -gdb tcp::9000

debug: build
	bochs -dbg
