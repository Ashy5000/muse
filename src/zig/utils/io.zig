pub const Port = u16;

pub fn out32(comptime port: Port, data: u32) void {
    asm volatile (
        \\ mov %[data], %%eax
        \\ mov %[port], %%dx
        \\ outl %%eax, (%%dx)
        :
        : [data] "r" (data),
          [port] "r" (port),
        : .{ .eax = true, .dx = true }
    );
}

pub fn in32(comptime port: Port) u32 {
    var res: u32 = undefined;
    asm (
        \\ mov %[port], %%dx
        \\ inl (%%dx), %[data]
        : [data] "=r" (res),
        : [port] "r" (port),
        : .{ .dx = true }
    );
    return res;
}
