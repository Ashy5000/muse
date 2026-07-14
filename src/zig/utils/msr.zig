const modules = @import("../modules.zig");
const cpuid = @import("../cpuid.zig");

pub const MSR = u32;

pub fn setMSR(msr: MSR, data: u64) void {
    asm volatile ("wrmsr"
        :
        : [msr] "{ecx}" (msr),
          [lo] "{eax}" (@as(u32, @truncate(data))),
          [hi] "{edx}" (@as(u32, @intCast(data >> 32))),
    );
}

pub fn getMSR(msr: MSR) u64 {
    var lo: u32 = undefined;
    var hi: u32 = undefined;
    asm volatile ("rdmsr"
        : [lo] "={eax}" (lo),
          [hi] "={edx}" (hi),
        : [msr] "{ecx}" (msr),
    );
    return lo | (@as(u64, hi) << 32);
}

fn init() modules.ModuleInitError!void {
    if (!cpuid.cpu_features.?.msr) {
        return error.ModuleUnsupported;
    }
}

pub var mod: modules.Module = .{
    .name = "msr",
    .init = init,
    .deps = &.{&cpuid.mod},
};
