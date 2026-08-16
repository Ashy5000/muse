const std = @import("std");
const timer = @import("subsystems/timer.zig");
const scheduler = @import("scheduler.zig");
const cpu = @import("smp/cpu.zig");

const slice_duration: timer.IntervalPico = 1e9;
var slice_time: timer.IntervalPico = 0;

const SleepingTask = struct {
    countdown: timer.IntervalPico,
    task: *scheduler.Task,
    next: ?*SleepingTask,
};

var sleep_pool = std.heap.MemoryPool(SleepingTask).empty;
var sleep_queue: ?*SleepingTask = null;

const modules = @import("modules.zig");
const TickError = std.mem.Allocator.Error || modules.InitError;

pub fn sleep(gpa: std.mem.Allocator, time: timer.IntervalPico) TickError!void {
    const queue = &(try cpu.getActiveCPU()).queue;
    @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .in_use, .monotonic);
    defer @atomicStore(@TypeOf(queue.sync_status), &queue.sync_status, .available, .release);
    const sleeping: *SleepingTask = try sleep_pool.create(gpa);
    sleeping.* = .{
        .countdown = time,
        .task = queue.active,
        .next = sleep_queue,
    };
    sleep_queue = sleeping;
    try scheduler.schedule();
}

pub fn tick() TickError!void {
    var sleeping_task: ?*SleepingTask = sleep_queue;
    var prev: ?*SleepingTask = null;
    while (sleeping_task) |task| {
        const next = task.next;
        if (task.countdown <= timer.tick_period) {
            try scheduler.push(task.task);
            sleep_pool.destroy(task);
            if (prev) |p| {
                p.next = next;
            } else {
                sleep_queue = next;
            }
        } else {
            task.countdown -= timer.tick_period;
            prev = task;
        }
        sleeping_task = next;
    }
    slice_time += timer.tick_period;
    if (slice_time >= slice_duration) {
        slice_time = 0;
        try scheduler.preempt();
    }
}
