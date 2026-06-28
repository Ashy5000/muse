const interrupts = @import("../interrupts.zig");
const idt = @import("../arch.zig").idt;
const panic = @import("../panic.zig");

const PFCode = packed struct(u32) {
    present: bool,
    write: bool,
    user: bool,
    reserved_write: bool,
    instruction: bool,
    prot_key: bool,
    shadow_stack: bool,
    rsvd0: u8,
    sgx: bool,
    rsvd1: u16,
};

fn pageFault(_: *anyopaque, code_int: usize) callconv(idt.int_callconv) void {
    const code: PFCode = @bitCast(@as(u32, @intCast(code_int)));
    const write = if (code.write) "write to" else "read from";
    const present = if (code.present) "n unauthorized" else " non-present";
    var addr: usize = undefined;
    asm ("mov %%cr2, %[addr]"
        : [addr] "=r" (addr),
    );
    panic.kpanic(
        \\#PF (code: 0x{x:0>8})
        \\Caused by a {s} a{s} region at 0x{x:0>8}.
        \\
    , .{ code_int, write, present, addr });
}

pub var isr_info: interrupts.ISRInfo = .{
    .irq = 0xE,
    .isr = pageFault,
};
