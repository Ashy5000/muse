pub const Port = u16;

pub fn out32(port: Port, data: u32) void {
    asm volatile ("outl %[data], %[port]"
        :
        : [data] "a" (data),
          [port] "Nd" (port),
    );
}

pub fn in32(port: Port) u32 {
    var res: u32 = undefined;
    asm volatile ("inl %[port], %[data]"
        : [data] "=a" (res),
        : [port] "Nd" (port),
    );
}
