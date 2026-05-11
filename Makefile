BUILD_DIR = build
KERNEL_SRC = kernel
DRIVER_SRC = drivers
LIBC_SRC = libc/lib
RUNTIME_SRC = libc/runtime
CROSS_ROOT = $(HOME)/opt/cross/bin
CC = $(CROSS_ROOT)/i686-muse-gcc
AS = $(CROSS_ROOT)/i686-muse-as
LD = $(CROSS_ROOT)/i686-muse-ld
AR = $(CROSS_ROOT)/i686-muse-ar
SRC_DIRS = $(KERNEL_SRC) $(DRIVER_SRC) $(LIBC_SRC)
KERNEL_C_SOURCES = $(wildcard $(KERNEL_SRC)/*.c)
DRIVER_C_SOURCES = $(wildcard $(DRIVER_SRC)/*.c)
KERNEL_ASM_SOURCES = $(wildcard $(KERNEL_SRC)/*.asm)
DRIVER_ASM_SOURCES = $(wildcard $(DRIVER_SRC)/*.asm)
LIBC_C_SOURCES = $(wildcard $(LIBC_SRC)/*.c)
LIBC_ASM_SOURCES = $(wildcard $(LIBC_SRC)/*.asm)
RUNTIME_ASM_SOURCES = $(wildcard $(RUNTIME_SRC)/*.asm)
KERNEL_OBJS = $(patsubst $(KERNEL_SRC)/%.c, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_C_SOURCES)) $(patsubst $(KERNEL_SRC)/%.asm, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_ASM_SOURCES))
DRIVER_OBJS = $(patsubst $(DRIVER_SRC)/%.c, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_C_SOURCES)) $(patsubst $(DRIVER_SRC)/%.asm, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_ASM_SOURCES))
LIBC_OBJS = $(patsubst $(LIBC_SRC)/%.c, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_C_SOURCES)) $(patsubst $(LIBC_SRC)/%.asm, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_ASM_SOURCES))
LIB_DIR = fs/usr/lib
RUNTIME_OBJS = $(patsubst $(RUNTIME_SRC)/%.asm, $(LIB_DIR)/%.o, $(RUNTIME_ASM_SOURCES))
LIBC_PATH = $(LIB_DIR)/libc.a

define compile-c =
$(CC) -ffreestanding -c $< -o $@ -Os -g
endef

define assemble =
$(AS) $< -o $@ -g
endef

$(BUILD_DIR)/boot_sect.bin: bootloader/
	cd bootloader; nasm boot_sect.asm -f bin -o ../$(BUILD_DIR)boot_sect.bin

$(BUILD_DIR)/%.o: %.c
	$(compile-c)

kernel_entry.o: kernel_entry.c
	$(compile-c)

$(BUILD_DIR)/%.o: %.asm
	$(assemble)

$(LIB_DIR)/%.o: $(RUNTIME_SRC)/%.asm
	$(assemble)

$(BUILD_DIR)/kernel.bin: kernel_entry.o $(KERNEL_OBJS) $(DRIVER_OBJS)
	$(LD) -o $@ -Ttext 0x1000 $^ $(HOME)/opt/cross/lib/gcc/i686-muse/17.0.0/libgcc.a --oformat binary --entry main -g

$(LIBC_PATH): $(LIBC_OBJS)
	$(AR) rcs $@ $^

disk.bin: $(BUILD_DIR)/boot_sect.bin $(BUILD_DIR)/kernel.bin fs
	dd if=/dev/zero of=disk.bin bs=512 count=131072
	sudo sh -c "yes | parted disk.bin mktable GPT"
	sudo parted disk.bin mkpart MUSEKRN 2048s 4095s -a none
	sudo parted disk.bin mkpart MUSEFS 4096s 131001s -a none
	dd if=boot_sect.bin of=disk.bin bs=1 count=446 conv=notrunc
	dd if=kernel.bin of=disk.bin bs=512 seek=2048 count=64 conv=notrunc
	sudo umount /dev/loop0 -q; exit 0
	sudo losetup -D
	sudo losetup /dev/loop0 disk.bin -o 2097152
	sudo mke2fs /dev/loop0 63453k
	sudo mount /dev/loop0 /mnt
	sudo rm -rf /mnt/lost+found
	sudo cp -r fs/* /mnt/
	sync

runtime: $(RUNTIME_OBJS)

libc: $(LIBC_PATH)

build: disk.bin libc runtime

run: build
	qemu-system-i386 -drive format=raw,file=disk.bin -no-reboot -no-shutdown -gdb tcp::9000

debug: build
	bochs -dbg

clean:
	rm $(BUILD_DIR)/*.o

.PHONY: runtime libc build run debug clean
