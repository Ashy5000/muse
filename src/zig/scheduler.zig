const std = @import("std");
const cpu = @import("smp/cpu.zig");
const contextSwitch = @import("arch.zig").contextSwitch;
const console = @import("console.zig");

pub const Task = extern struct {
    esp: usize,
    next: ?*Task,
    status: std.atomic.Value(enum(u8) { active, blocked }),
};

pub var task_pool: std.heap.MemoryPool(Task) = .empty;

pub const Queue = struct {
    sync_status: std.atomic.Value(enum(u8) {
        available,
        in_use,
    }),
    active: *Task,
    list: ?struct {
        next: *Task,
        last: *Task,
    },
};

const modules = @import("modules.zig");

pub fn push(task: *Task) modules.InitError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    queue.sync_status.store(.in_use, .seq_cst);
    defer queue.sync_status.store(.available, .release);
    task.next = null;
    if (queue.list) |*list| {
        list.last.next = task;
        list.last = task;
    } else {
        queue.list = .{
            .next = task,
            .last = task,
        };
    }
}

pub fn schedule() modules.InitError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    if (queue.list) |*list| {
        const old = queue.active;
        const new = list.next;
        if (new.next) |next| {
            list.next = next;
        } else {
            console.print("1 task only.\n", .{});
            queue.list = null;
        }
        queue.active = new;
        old.status.store(.blocked, .release);
        contextSwitch.contextSwitch(old, new);
        old.status.store(.active, .release);
    }
}

pub fn preempt() modules.InitError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    if (queue.sync_status.cmpxchgStrong(
        .available,
        .in_use,
        .seq_cst,
        .monotonic,
    )) |_| {
        return;
    }
    if (queue.list) |*list| {
        queue.active.next = null;
        list.last.next = queue.active;
        list.last = queue.active;
        queue.active = list.next;
        list.next = queue.active.next.?;
        queue.sync_status.store(.available, .release);
        contextSwitch.contextSwitch(list.last, queue.active);
    } else {
        queue.sync_status.store(.available, .release);
    }
}

pub fn terminate() noreturn {
    const heap = @import("alloc/heap.zig");
    std.debug.panic("error while failing task: {}", .{canerr: {
        const queue = &(cpu.getActiveCPU() catch |err| break :canerr err).queue;
        queue.sync_status.store(.in_use, .seq_cst);
        errdefer queue.sync_status.store(.available, .release);
        if (queue.list) |*list| {
            const old = queue.active;
            const new = list.next;
            if (new.next) |next|
                list.next = next
            else
                queue.list = null;
            queue.active = new;
            old.status.store(.blocked, .release);
            var old_cp = old.*;
            (heap.mod.data() catch |err| break :canerr err).destroy(old);
            queue.sync_status.store(.available, .release);
            contextSwitch.contextSwitch(&old_cp, new);
            unreachable;
        } else {
            std.debug.panic("last task exited", .{});
        }
    }});
}

pub fn fail(comptime fmt: []const u8, args: anytype) noreturn {
    console.print(fmt, args);
    terminate();
}
