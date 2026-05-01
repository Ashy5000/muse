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
	dd if=/dev/zero of=disk.bin bs=512 count=131072
	sudo sh -c "yes | parted disk.bin mktable GPT"
	sudo parted disk.bin mkpart MUSEKRN 2048s 4095s
	sudo parted disk.bin mkpart MUSEFS 4096s 131000s 
	sudo losetup -D
	sudo losetup /dev/loop0 disk.bin
	sudo sh -c "cat boot_sect.bin > /dev/loop0"
	sudo losetup /dev/loop1 disk.bin -o 2097152
	sudo mke2fs /dev/loop1
	sudo losetup -d /dev/loop1
	sudo losetup /dev/loop1 disk.bin -o 1048576
	sudo sh -c "cat kernel.bin > /dev/loop1"

build: disk.bin

run: build
	qemu-system-i386 -drive format=raw,file=disk.bin -no-reboot -no-shutdown -gdb tcp::9000

debug: build
	bochs -dbg
