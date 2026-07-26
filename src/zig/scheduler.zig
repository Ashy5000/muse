const cpu = @import("smp/cpu.zig");
const contextSwitch = @import("arch/x86_64/contextSwitch.zig");

pub const Task = struct {
    esp: usize,
    next: ?*Task,
};

pub const Queue = struct {
    active: *Task,
    next: ?*Task,
    last: ?*Task,
};

pub fn push(task: *Task) void {
    const queue: *Queue = &cpu.getActiveCPU().queue;
    task.next = queue.next;
    queue.next = task;
    if (queue.last) |_| {} else {
        queue.last = task;
    }
}

pub fn schedule() void {
    const queue: *Queue = &cpu.getActiveCPU().queue;
    if (queue.next) |next| {
        const old = queue.active;
        queue.next = next.next;
        queue.active = next;
        contextSwitch.contextSwitch(old, next);
    }
}

pub fn preempt() void {
    const queue: *Queue = &cpu.getActiveCPU().queue;
    if (queue.next) |next| {
        queue.active.next = null;
        queue.last.?.next = queue.active; // TODO: Wrap next and last in one optional
        queue.last = queue.active;
        queue.next = next.next;
        queue.active = next;
        contextSwitch.contextSwitch(queue.last.?, next);
    }
}
