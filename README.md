A hobby OS, purely for the purpose of having fun and learning stuff (handwritten for this reason).

**Features**
- Memory management
- Paging
- Multiboot2
- Prekernel
- Higher-half kernel
- Interrupts
- Scheduling
- Userspace
- Synchronization primitives
- VFS
- Console w/ PSF font rendering

**Drivers**
- Ext2
- ATA
- HPET
- PS/2 controller & keyboard
- PCI
- VGA framebuffer

**Toolchain Dependencies**
- Basic coreutils
- Make
- Cross-compiler for target architecture and OS
- GRUB
- EFI image
- Filesystem utilities
    - e2tools
    - dosfstools
    - mtools
- QEMU (for testing in a VM)
