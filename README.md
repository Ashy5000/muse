A hobby kernel and bootloader. Supports interrupts, paging, memory allocation, and a basic scheduler. Regularly tested on both QEMU and BOCHS emulators.

**WARNING: The compilation toolchain for this project is designed *exclusively* for Linux. Attempting to build this project on *any other OS* will (most likely) not work. There are GCC-specific compiler directives that will not work with other compilers, so `clang` or `nvcc` will not function properly.**
