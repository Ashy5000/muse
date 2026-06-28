const builtin = @import("builtin");
pub const paging = switch (builtin.target.cpu.arch) {
    .x86 => @import("arch/x86/paging.zig"),
    else => @compileError("paging not supported for target"),
};
pub const idt = switch (builtin.target.cpu.arch) {
    .x86 => @import("arch/x86/idt.zig"),
    else => @compileError("paging not supported for target"),
};
