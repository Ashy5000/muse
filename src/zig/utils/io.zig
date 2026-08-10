pub const Port = u16;

pub fn out(
    comptime bits: u16,
    port: Port,
    data: @Int(.unsigned, bits),
) void {
    switch (bits) {
        8 => out8(port, data),
        32 => out32(port, data),
        else => @compileError("unsupported I/O write bit width"),
    }
}

pub fn in(comptime bits: u16, port: Port) @Int(.unsigned, bits) {
    return switch (bits) {
        8 => in8(port),
        16 => in16(port),
        32 => in32(port),
        else => @compileError("unsupported I/O read bit width"),
    };
}

fn out8(port: Port, data: u8) void {
    asm volatile (
        \\ outb %%al, (%%dx)
        :
        : [data] "{al}" (data),
          [port] "{dx}" (port),
    );
}

fn in8(port: Port) u8 {
    return asm volatile (
        \\ inb (%%dx), %%al
        : [data] "={al}" (-> u8),
        : [port] "{dx}" (port),
    );
}

fn in16(port: Port) u16 {
    return asm volatile (
        \\ inw (%%dx), %%ax
        : [data] "={ax}" (-> u16),
        : [port] "{dx}" (port),
    );
}

fn out32(port: Port, data: u32) void {
    asm volatile (
        \\ outl %%eax, (%%dx)
        :
        : [data] "{eax}" (data),
          [port] "{dx}" (port),
    );
}

fn in32(port: Port) u32 {
    return asm volatile (
        \\ inl (%%dx), %%eax
        : [data] "={eax}" (-> u32),
        : [port] "{dx}" (port),
    );
}
