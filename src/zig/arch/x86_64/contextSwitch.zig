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

pub fn createKernelTask(entry_point: *const fn () void) virtual.MapError!scheduler.Task {
    const stack_phys = @as([*]u8, @ptrFromInt(try pmm.pmmAlloc(paging.page_size)))[0..paging.page_size];
    const stack = try virtual.mapPhysObj(stack_phys, .{});
    // Return address plus 6 preserved registers
    const stack_size = 7 * @sizeOf(usize);
    @as(**const fn () void, @ptrFromInt(@intFromPtr(stack.ptr) + paging.page_size - @sizeOf(usize))).* = entry_point;
    return .{
        .esp = @intFromPtr(stack.ptr) + paging.page_size - stack_size,
        .next = null,
    };
}
