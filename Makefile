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
	sudo parted disk.bin mkpart MUSEKRN 2048s 4095s -a none
	sudo parted disk.bin mkpart MUSEFS 4096s 131001s -a none
	dd if=boot_sect.bin of=disk.bin bs=1 count=446 conv=notrunc
	sudo losetup -D
	sudo losetup /dev/loop0 disk.bin -o 2097152
	sudo mke2fs /dev/loop0 63453k
	dd if=kernel.bin of=disk.bin bs=512 seek=2048 count=64 conv=notrunc

build: disk.bin

run: build
	qemu-system-i386 -drive format=raw,file=disk.bin -no-reboot -no-shutdown -gdb tcp::9000

debug: build
	bochs -dbg
