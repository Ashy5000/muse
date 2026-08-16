const modules = @import("../modules.zig");
const cpuid = @import("../cpuid.zig");

pub const MSR = u32;

pub fn setMSR(msr: MSR, data: u64) modules.InitError!void {
    if (!(try cpuid.mod.data(*cpuid.Info)).features.msr) {
        return error.Unsupported;
    }
    asm volatile ("wrmsr"
        :
        : [msr] "{ecx}" (msr),
          [lo] "{eax}" (@as(u32, @truncate(data))),
          [hi] "{edx}" (@as(u32, @intCast(data >> 32))),
    );
}

pub fn getMSR(msr: MSR) modules.InitError!u64 {
    if (!(try cpuid.mod.data(*cpuid.Info)).features.msr) {
        return error.Unsupported;
    }
    var lo: u32 = undefined;
    var hi: u32 = undefined;
    asm volatile ("rdmsr"
        : [lo] "={eax}" (lo),
          [hi] "={edx}" (hi),
        : [msr] "{ecx}" (msr),
    );
    return lo | (@as(u64, hi) << 32);
}
