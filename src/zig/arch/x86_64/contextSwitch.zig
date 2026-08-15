const scheduler = @import("../../scheduler.zig");
const pmm = @import("../../alloc/pmm.zig");
const paging = @import("../../arch.zig").paging;
const virtual = @import("../../virtual.zig");

pub extern fn contextSwitch(active: *scheduler.Task, next: *scheduler.Task) void;

comptime {
    asm (
        \\ .global contextSwitch
        \\ contextSwitch:
        \\     push %rbx
        \\     push %rbp
        \\     push %r12
        \\     push %r13
        \\     push %r14
        \\     push %r15
        \\     mov %rsp, (%rdi)
        \\     mov (%rsi), %rsp
        \\     pop %r15
        \\     pop %r14
        \\     pop %r13
        \\     pop %r12
        \\     pop %rbp
        \\     pop %rbx
        \\     ret
    );
}

pub fn createKernelTask(
    entry_point: *const fn () void,
) virtual.MapError!scheduler.Task {
    const stack_cap = 0x10000;
    const stack_phys = try pmm.pmmAlloc(stack_cap)[0..stack_cap];
    const stack = try virtual.mapPhysObj(stack_phys, .{});
    // Return address plus 6 preserved registers
    const stack_size = 7 * @sizeOf(usize);
    @as(**const fn () void, @ptrFromInt(@intFromPtr(stack.ptr) + stack_cap - @sizeOf(usize))).* = entry_point;
    return .{
        .esp = @intFromPtr(stack.ptr) + stack_cap - stack_size,
        .next = null,
    };
}
