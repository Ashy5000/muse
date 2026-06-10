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
DEPS_DIR = deps

CROSS_ROOT = $(HOME)/opt/cross/bin
CC = $(CROSS_ROOT)/i686-muse-gcc
AS = $(CROSS_ROOT)/i686-muse-as
LD = $(CROSS_ROOT)/i686-muse-ld
AR = $(CROSS_ROOT)/i686-muse-ar
CPY = $(CROSS_ROOT)/i686-muse-objcopy
CFLAGS = -Wall -Wextra -Werror -O0

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
LIBC_DEPS = $(patsubst $(LIBC_SRC)/%.c, $(BUILD_DIR)/$(LIBC_SRC)/%.d, $(LIBC_C_SOURCES))
ALL_DEPS = $(KERNEL_DEPS) $(DRIVER_DEPS) $(LIBC_DEPS)

KERNEL_OBJS = $(patsubst $(KERNEL_SRC)/%.c, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_C_SOURCES)) $(patsubst $(KERNEL_SRC)/%.asm, $(BUILD_DIR)/$(KERNEL_SRC)/%.o, $(KERNEL_ASM_SOURCES))
DRIVER_OBJS = $(patsubst $(DRIVER_SRC)/%.c, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_C_SOURCES)) $(patsubst $(DRIVER_SRC)/%.asm, $(BUILD_DIR)/$(DRIVER_SRC)/%.o, $(DRIVER_ASM_SOURCES))
LIBC_OBJS = $(patsubst $(LIBC_SRC)/%.c, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_C_SOURCES)) $(patsubst $(LIBC_SRC)/%.asm, $(BUILD_DIR)/$(LIBC_SRC)/%.o, $(LIBC_ASM_SOURCES))
RUNTIME_OBJS = $(patsubst $(RUNTIME_SRC)/%.asm, $(LIB_DIR)/%.o, $(RUNTIME_ASM_SOURCES))

define compile-c =
$(CC) $(CFLAGS) -ffreestanding -MMD -MP -c $< -o $@ -g
endef

define compile-usr-c =
$(CC) $(CFLAGS) -MMD -MP -c $< -o $@ -g
endef

define assemble =
$(AS) $< -o $@
endef

all: build

.PHONY: all libc build run debug clean todo

build: $(BUILD_DIR)/disk.img

run: build
	qemu-system-i386 -drive format=raw,file=$(BUILD_DIR)/disk.img \
	-drive if=pflash,format=raw,readonly=on,file=$(DEPS_DIR)/bios32.bin \
	-no-reboot -no-shutdown

debug: build
	qemu-system-i386 -drive format=raw,file=$(BUILD_DIR)/disk.img \
	-drive if=pflash,format=raw,readonly=on,file=$(DEPS_DIR)/bios32.bin \
	-no-reboot -no-shutdown -s -S

clean:
	rm -rf $(BUILD_DIR)/*.o

todo:
	-@for file in $(ALL_SRC:Makefile_grub=); do grep -F -H -e TODO -e FIXME $$file; done; true

$(BUILD_DIR)/disk.img: $(BUILD_DIR)/esp.img $(BUILD_DIR)/rootfs.img
	truncate -s 128M $@
	dd if=/dev/zero of=$@ bs=512 count=262144
	sgdisk $@ -n 1:2048:+64M -t 1:ef00 -n 2:0:0 -t 2:8300
	dd if=$(BUILD_DIR)/esp.img of=$@ bs=512 seek=2048 conv=notrunc
	dd if=$(BUILD_DIR)/rootfs.img of=$@ bs=512 seek=133120 conv=notrunc

$(BUILD_DIR)/rootfs.img:
	truncate -s 64M $@
	mke2fs -t ext2 -F $@

$(BUILD_DIR)/esp.img: $(BUILD_DIR)/muse $(BUILD_DIR)/BOOTIA32.EFI grub.cfg
	truncate -s 64M $@
	mkfs.fat -F32 $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mmd -i $@ ::/boot
	mmd -i $@ ::/boot/grub
	mcopy -i $@ $(BUILD_DIR)/BOOTIA32.EFI ::/EFI/BOOT/
	mcopy -i $@ grub.cfg ::/boot/grub
	mcopy -i $@ $(BUILD_DIR)/muse ::/boot

$(BUILD_DIR)/BOOTIA32.EFI:
	grub-mkimage -p /boot/grub -O i386-efi -o $@ fat part_gpt ext2 multiboot2 configfile all_video

$(BUILD_DIR)/muse: boot.o $(KERNEL_OBJS) $(DRIVER_OBJS)
	$(CC) -T linker.ld -o $@ -ffreestanding -O1 -nostdlib $^ -lgcc -g
	$(CPY) --only-keep-debug $@ $(BUILD_DIR)/muse.sym
	$(CPY) --strip-debug $@

# $(BUILD_DIR)/font.o: $(DEPS_DIR)/font.psf
# 	$(CPY) -O elf32-i386 -I binary $< $@

-include $(ALL_DEPS)

$(INCLUDE_DIR): $(LIBC_INCLUDE)
	rm -rf $(INCLUDE_DIR)/*
	cp -r $(LIBC_INCLUDE) $(INCLUDE_DIR)/..

$(LIBC_PATH): $(LIBC_OBJS) 
	$(AR) rcs $@ $^

$(BUILD_DIR)/$(LIBC_SRC)/%.o: $(LIBC_SRC)/%.c Makefile_grub
	$(compile-usr-c)

$(BUILD_DIR)/%.o: %.c Makefile
	$(compile-c)

boot.o: boot.S
	$(assemble)

$(BUILD_DIR)/%.o: %.asm
	$(assemble)

$(LIB_DIR)/%.o: $(RUNTIME_SRC)/%.asm
	$(assemble)

libc: $(LIBC_PATH)
