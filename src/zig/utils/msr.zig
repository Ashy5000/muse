pub const MSR = u32;

pub fn setMSR(msr: MSR, data: u64) void {
    asm volatile (
        \\ mov %[msr], %%ecx
        \\ wrmsr
        :
        : [msr] "r" (msr),
          [data] "A" (data),
        : .{ .ecx = true, }
    );
}
