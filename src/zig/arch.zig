const builtin = @import("builtin");

/// A type which provides paging support for the target system.
pub const paging = switch (builtin.target.cpu.arch) {
    .x86 => @import("arch/x86/paging.zig"),
    .x86_64 => @import("arch/x86_64/paging.zig"),
    else => @compileError("paging not supported for target"),
};

/// A type which provides IDT support for the target system.
pub const idt = switch (builtin.target.cpu.arch) {
    .x86 => @import("arch/x86/idt.zig"),
    .x86_64 => @import("arch/x86_64/idt.zig"),
    else => @compileError("IDT not supported for target"),
};

/// A type which provides register dump support for the target system.
pub const dump = switch (builtin.target.cpu.arch) {
    .x86 => @import("arch/x86/dump.zig"),
    .x86_64 => @import("arch/x86_64/dump.zig"),
    else => @compileError("Register dumps not supported for target"),
};

pub const contextSwitch = switch (builtin.target.cpu.arch) {
    .x86_64 => @import("arch/x86_64/contextSwitch.zig"),
    else => @compileError("Context switching not supported for target"),
};
