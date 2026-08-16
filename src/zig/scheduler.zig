const std = @import("std");
const cpu = @import("smp/cpu.zig");
const contextSwitch = @import("arch/x86_64/contextSwitch.zig");

pub const Task = struct {
    esp: usize,
    next: ?*Task,
};

pub const Queue = struct {
    sync_status: enum {
        available,
        in_use,
    } align(std.atomic.cache_line),
    active: *Task,
    list: ?struct {
        next: *Task,
        last: *Task,
    },
};

const modules = @import("modules.zig");

pub fn push(task: *Task) modules.InitError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .in_use, .monotonic);
    defer @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .available, .release);
    if (queue.list) |*list| {
        task.next = null;
        list.last.next = task;
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
            queue.list = null;
        }
        queue.active = new;
        contextSwitch.contextSwitch(old, new);
    }
}

pub fn preempt() modules.InitError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    if (@atomicLoad(@TypeOf(queue.sync_status), &queue.sync_status, .acquire) != .available) {
        return;
    }
    @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .in_use, .monotonic);
    defer @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .available, .release);
    if (queue.list) |*list| {
        queue.active.next = null;
        list.last.next = queue.active;
        list.last = queue.active;
        queue.active = list.next;
        list.next = queue.active.next.?;
        contextSwitch.contextSwitch(list.last, queue.active);
    }
}
