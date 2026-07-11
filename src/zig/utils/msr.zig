const modules = @import("../modules.zig");
const cpuid = @import("../cpuid.zig");

pub const MSR = u32;

pub fn setMSR(msr: MSR, data: u64) void {
    asm volatile (
        \\ wrmsr
        :
        : [msr] "{ecx}" (msr),
          [data] "A" (data),
    );
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
