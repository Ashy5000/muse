pub const Port = u16;

pub fn out8(port: Port, data: u8) void {
    asm volatile (
        \\ outb %%al, (%%dx)
        :
        : [data] "{al}" (data),
          [port] "{dx}" (port),
    );
}

pub fn in8(port: Port) u8 {
    return asm volatile (
        \\ inb (%%dx), %%al
        : [data] "={al}" (-> u8),
        : [port] "{dx}" (port),
    );
}

pub fn in16(port: Port) u16 {
    return asm volatile (
        \\ inw (%%dx), %%ax
        : [data] "={ax}" (-> u16),
        : [port] "{dx}" (port),
    );
}

pub fn out32(port: Port, data: u32) void {
    asm volatile (
        \\ outl %%eax, (%%dx)
        :
        : [data] "{eax}" (data),
          [port] "{dx}" (port),
    );
}

pub fn in32(port: Port) u32 {
    return asm volatile (
        \\ inl (%%dx), %%eax
        : [data] "={eax}" (-> u32),
        : [port] "{dx}" (port),
    );
}
