pub const Port = u16;

pub fn out8(comptime port: Port, data: u8) void {
    asm volatile (
        \\ outb %%al, (%%dx)
        :
        : [data] "{al}" (data),
          [port] "{dx}" (port),
    );
}

pub fn out32(comptime port: Port, data: u32) void {
    asm volatile (
        \\ outl %%eax, (%%dx)
        :
        : [data] "{eax}" (data),
          [port] "{dx}" (port),
    );
}

pub fn in32(comptime port: Port) u32 {
    var res: u32 = undefined;
    asm (
        \\ inl (%%dx), %[data]
        : [data] "=r" (res),
        : [port] "{dx}" (port),
    );
    return res;
}
