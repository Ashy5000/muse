BUILD_DIR = build
KERNEL_SRC = kernel
DRIVER_SRC = drivers
LIBC_SRC = libc/lib
RUNTIME_SRC = libc/runtime
LIB_DIR = fs/usr/lib
LIBC_INCLUDE = libc/include
INCLUDE_DIR = fs/usr/include
LIBC_PATH = $(LIB_DIR)/libc.a
SRC_DIRS = $(KERNEL_SRC) $(DRIVER_SRC) $(LIBC_SRC)

CROSS_ROOT = $(HOME)/opt/cross/bin
CC = $(CROSS_ROOT)/i686-muse-gcc
AS = $(CROSS_ROOT)/i686-muse-as
LD = $(CROSS_ROOT)/i686-muse-ld
AR = $(CROSS_ROOT)/i686-muse-ar
CFLAGS = -Wall -Werror -Wextra -O1

KERNEL_C_SOURCES = $(wildcard $(KERNEL_SRC)/*.c)
DRIVER_C_SOURCES = $(wildcard $(DRIVER_SRC)/*.c)
LIBC_C_SOURCES = $(wildcard $(LIBC_SRC)/*.c)
ALL_SRC = $(KERNEL_C_SOURCES) $(DRIVER_C_SOURCES) $(KERNEL_ASM_SOURCES) $(DRIVER_ASM_SOURCES) $(LIBC_C_SOURCES) $(LIBC_ASM_SOURCES) $(RUNTIME_ASM_SOURCES)

KERNEL_ASM_SOURCES = $(wildcard $(KERNEL_SRC)/*.asm)
DRIVER_ASM_SOURCES = $(wildcard $(DRIVER_SRC)/*.asm)
LIBC_ASM_SOURCES = $(wildcard $(LIBC_SRC)/*.asm)
RUNTIME_ASM_SOURCES = $(wildcard $(RUNTIME_SRC)/*.asm)

KERNEL_DEPS = $(patsubst $(KERNEL_SRC)/%.c, $(BUILD_DIR)/$(KERNEL_SRC)/%.d, $(KERNEL_C_SOURCES))
DRIVER_DEPS = $(patsubst $(DRIVER_SRC)/%.c, $(BUILD_DIR)/$(DRIVER_SRC)/%.d, $(DRIVER_C_SOURCES))
LIBC_DEPS = $(patsubst $(LIBC_SRC)/%.c, $(BUILD_DIR)/$(LIBC_SRC)/%.dm, $(LIBC_C_SOURCES))
ALL_DEPS = $(KERNEL_DEPS) $(DRIVER_DEPS) $(LIBC_DEPS)

KERNEL_OBJS = $(patsubst $(KERNEL_SRC)/%.c, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_C_SOURCES)) $(patsubst $(KERNEL_SRC)/%.asm, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_ASM_SOURCES))
DRIVER_OBJS = $(patsubst $(DRIVER_SRC)/%.c, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_C_SOURCES)) $(patsubst $(DRIVER_SRC)/%.asm, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_ASM_SOURCES))
LIBC_OBJS = $(patsubst $(LIBC_SRC)/%.c, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_C_SOURCES)) $(patsubst $(LIBC_SRC)/%.asm, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_ASM_SOURCES))
RUNTIME_OBJS = $(patsubst $(RUNTIME_SRC)/%.asm, $(LIB_DIR)/%.o, $(RUNTIME_ASM_SOURCES))

define compile-c =
$(CC) $(CFLAGS) -ffreestanding -MMD -MP -c $< -o $@
endef

define compile-usr-c =
$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
endef

define assemble =
$(AS) $< -o $@ -g
endef

all: build

.PHONY: all libc build run debug clean todo

build: disk.bin

run: build
	qemu-system-i386 -drive format=raw,file=disk.bin -no-reboot -no-shutdown -gdb tcp::9000

debug: build
	bochs -dbg

clean:
	rm $(BUILD_DIR)/*.o

todo:
	-@for file in $(ALL_SRC:Makefile=); do grep -F -H -e TODO -e FIXME $$file; done; true

disk.bin: $(BUILD_DIR)/boot_sect.bin $(BUILD_DIR)/kernel.bin $(INCLUDE_DIR) $(LIBC_PATH) $(RUNTIME_OBJS)
	dd if=/dev/zero of=disk.bin bs=512 count=131072
	sudo sh -c "yes | parted disk.bin mktable GPT"
	sudo parted disk.bin mkpart MUSEKRN 2048s 4095s -a none
	sudo parted disk.bin mkpart MUSEFS 4096s 131001s -a none
	dd if=$(BUILD_DIR)/boot_sect.bin of=disk.bin bs=1 count=446 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=disk.bin bs=512 seek=2048 count=64 conv=notrunc
	-sudo umount /dev/loop0 -q
	sudo losetup -D
	sudo losetup /dev/loop0 disk.bin -o 2097152
	sudo sh -c "yes | mke2fs /dev/loop0 63453k"
	sudo mount /dev/loop0 /mnt
	sudo cp -r fs/* /mnt/
	sync

-include $(ALL_DEPS)

$(INCLUDE_DIR): $(LIBC_INCLUDE)
	rm -rf $(INCLUDE_DIR)/*
	cp -r $(LIBC_INCLUDE) $(INCLUDE_DIR)/..

$(LIBC_PATH): $(LIBC_OBJS) 
	$(AR) rcs $@ $^

$(BUILD_DIR)/boot_sect.bin: bootloader/
	cd bootloader; nasm boot_sect.asm -f bin -o ../$(BUILD_DIR)/boot_sect.bin

$(BUILD_DIR)/$(LIBC_SRC)/%.o: $(LIBC_SRC)/%.c Makefile
	$(compile-usr-c)

$(BUILD_DIR)/%.o: %.c Makefile
	$(compile-c)

kernel_entry.o: kernel_entry.c Makefile
	$(compile-c)

$(BUILD_DIR)/%.o: %.asm
	$(assemble)

$(LIB_DIR)/%.o: $(RUNTIME_SRC)/%.asm
	$(assemble)

$(BUILD_DIR)/kernel.bin: kernel_entry.o $(KERNEL_OBJS) $(DRIVER_OBJS)
	$(LD) -o $@ -Ttext 0x8000 $^ $(HOME)/opt/cross/lib/gcc/i686-muse/17.0.0/libgcc.a --oformat binary

libc: $(LIBC_PATH)
