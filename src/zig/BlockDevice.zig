const std = @import("std");
const modules = @import("modules.zig");
const scheduler = @import("scheduler.zig");
const cpu = @import("smp/cpu.zig");
const sync = @import("utils/sync.zig");
const console = @import("console.zig");
pub const Error = modules.InitError || error{IOFailed};

const BlockDevice = @This();

var runway: struct {
    dev: *anyopaque = undefined,
    mutex: sync.Mutex = .unlocked,
} = .{};

const vfs = @import("vfs.zig");

const DependencyInfo = struct {
    priority: Priority,
    seq: u32,
};

const TransferInfo = struct {
    completion_info: union(enum) {
        transfer: struct {
            bfrs: [][]u8,
            dir: vfs.Vnode.Direction,
            lba: u64,
        },
        dependency: DependencyInfo,
    },
    cancel: bool = false,
    task: *scheduler.Task,
    res: *Error!void,
};

const TransferFunction = *const fn (
    data: *anyopaque,
    bfrs: [][]u8,
    dir: vfs.Vnode.Direction,
    lba: u64,
) Error!void;

const Queue = struct {
    elems: []TransferInfo,
    pushMutex: sync.Mutex,
    seq_produce: u32,
    seq_consume: u32,

    fn push(
        self: *Queue,
        info: TransferInfo,
        /// What to do if the queue is unavailable (full). .block: wait
        /// until a spot frees up. .fail: cancel the operation and fail
        /// silently, returning void the same as a success.
        busy_policy: enum { block, fail },
    ) modules.InitError!?u32 {
        while (self.seq_produce - self.seq_consume >= 8)
            switch (busy_policy) {
                .block => try scheduler.preempt(),
                .fail => return null,
            };

        const seq = self.seq_produce;
        self.elems[seq % self.elems.len] = info;
        self.seq_produce += 1;
        return seq;
    }

    fn pop(self: *Queue) ?TransferInfo {
        if (self.seq_produce == self.seq_consume) return null;
        const idx = self.seq_consume % self.elems.len;
        self.seq_consume = self.seq_consume + 1;
        return self.elems[idx];
    }
};

data: *anyopaque,
transferRaw: TransferFunction,
queues: [3]Queue,

const CreationError = std.mem.Allocator.Error || modules.InitError || error{FileAlreadyExists};
pub fn new(
    data: *anyopaque,
    transferRaw: TransferFunction,
    queueLen: ?usize,
    gpa: std.mem.Allocator,
) CreationError!void {
    const default_queue_len = 64;

    const self = try gpa.create(BlockDevice);
    self.* = .{
        .data = data,
        .transferRaw = transferRaw,
        .queues = fill: {
            var res: [3]Queue = undefined;
            for (0..3) |i| {
                res[i] = .{
                    .elems = try gpa.alloc(TransferInfo, queueLen orelse default_queue_len),
                    .pushMutex = .unlocked,
                    .seq_produce = 0,
                    .seq_consume = 0,
                };
            }
            break :fill res;
        },
    };
    const contextSwitch = @import("arch.zig").contextSwitch;
    try runway.mutex.acquire();
    errdefer runway.mutex.release();
    runway.dev = self;
    const task = contextSwitch.createKernelTask(
        &BlockDevice.consumerEntry,
        gpa,
    ) catch return error.CriticalSystemFailure;
    try scheduler.push(task);

    const partition = @import("subsystems/partition.zig");
    partition.probePartitions(self) catch |err| switch (err) {
        error.CriticalSystemFailure => std.debug.panic(
            "partition probe caused critical system failure",
            .{},
        ),
        error.IOFailed => console.print(
            "warning: I/O error during partition probe\n",
            .{},
        ),
    };
}

const sector_size: usize = 512;

pub fn transferQueued(
    self: *BlockDevice,
    data: []u8,
    dir_p: vfs.Vnode.Direction,
    offset: usize,
    priority: Priority,
    mode: Mode,
    dep_info: ?DependencyInfo,
) vfs.Vnode.TransferError!?u32 {
    const heap = @import("alloc/heap.zig");
    const dir: vfs.Vnode.Direction = switch (dir_p) {
        .read => .read,
        .write => .write,
    };
    const sector_align = std.mem.Alignment.fromByteUnits(sector_size);
    const lba_start = offset / sector_size;

    const data_start = sector_align.forward(offset);
    const data_end = sector_align.backward(offset + data.len);
    const use_data = data_end > data_start;

    const pre_start = sector_align.backward(offset);
    const pre_end = data_start;
    const use_pre = pre_end > pre_start;

    const post_start = data_end;
    const post_end = sector_align.forward(offset + data.len);
    const use_post = post_end > post_start and (post_start != pre_start or data_start == offset);

    const gpa = heap.mod.data() catch return error.CriticalSystemFailure;
    const extra_bfrs: [][sector_size]u8 = gpa.alloc(
        [sector_size]u8,
        @as(
            usize,
            if (use_pre) 1 else 0,
        ) + @as(
            usize,
            if (use_post) 1 else 0,
        ),
    ) catch return error.CriticalSystemFailure;
    defer gpa.free(extra_bfrs);

    switch (dir) {
        .read => {
            var bfrs: [][]u8 = gpa.alloc(
                []u8,
                extra_bfrs.len + @as(usize, if (use_data) 1 else 0),
            ) catch return error.CriticalSystemFailure;
            defer gpa.free(bfrs);

            var idx: usize = 0;
            if (use_pre) {
                bfrs[idx] = &extra_bfrs[0];
                idx += 1;
            }
            if (use_data) {
                bfrs[idx] = data[data_start - offset .. data_end - offset];
                idx += 1;
            }
            if (use_post) {
                bfrs[idx] = &extra_bfrs[extra_bfrs.len - 1];
                idx += 1;
            }

            const res = self.produceTransfer(
                bfrs,
                .read,
                @intCast(lba_start),
                priority,
                mode,
                dep_info,
            ) catch |err| switch (err) {
                error.IOFailed => return error.IOFailed,
                else => return error.CriticalSystemFailure,
            };

            if (use_pre) {
                const len = @min(data.len, data_start - offset);
                @memcpy(
                    data[0..len],
                    extra_bfrs[0][offset - pre_start ..][0..len],
                );
            }
            if (use_post) {
                @memcpy(
                    data[data_end - offset ..],
                    extra_bfrs[extra_bfrs.len - 1][0 .. (offset + data.len) % sector_size],
                );
            }

            return res;
        },
        .write => std.debug.panic("Block device write unsupported", .{}),
    }
}

const Priority = enum {
    /// Normal priority transfer.
    standard,
    /// Low priority transfer, commonly used for prepare mode.
    secondary,
    /// Very low priority transfer, run by the VFS thread, which always
    /// blocks. Background transfers may only occasionally (or never)
    /// actually run.
    background,
};

const Mode = enum {
    /// "Normal" mode. Sumbits transfer to the queue and blocks
    /// until it is done.
    immediate,
    /// Submits transfer to the queue and returns immediately.
    async,
    /// Submits transfer to the queue and returns. When the transfer
    /// is complete, it will be recorded. Subsequent immediate mode
    /// transfers matching the recorded transfer will be ignored.
    prepare,
};

fn produceTransfer(
    self: *BlockDevice,
    bfrs: [][]u8,
    dir: vfs.Vnode.Direction,
    lba: u64,
    priority: Priority,
    mode: Mode,
    dep_info: ?DependencyInfo,
) Error!?u32 {
    const queue: *Queue = &self.queues[@intFromEnum(priority)];
    var err: Error!void = {};
    var res: ?u32 = null;
    {
        try queue.pushMutex.acquire();
        defer queue.pushMutex.release();
        const info: TransferInfo = .{
            .completion_info = if (dep_info) |dep| .{
                .dependency = dep,
            } else .{ .transfer = .{
                .bfrs = bfrs,
                .dir = dir,
                .lba = lba,
            } },
            .task = (try cpu.getActiveCPU()).queue.active,
            .res = &err,
        };
        res = queue.push(
            info,
            switch (mode) {
                .immediate, .async => .block,
                .prepare => .fail,
            },
        ) catch return error.CriticalSystemFailure;
    }
    if (mode == .immediate) try scheduler.schedule();
    err catch |e| return e;
    return res;
}

fn complete(self: *BlockDevice, info: *TransferInfo) Error!void {
    if (info.cancel) return;
    switch (info.completion_info) {
        .transfer => |transfer_info| self.transferRaw(
            self.data,
            transfer_info.bfrs,
            transfer_info.dir,
            transfer_info.lba,
        ) catch |err| {
            info.res.* = err;
            return err;
        },
        .dependency => |dependency_info| {
            const queue = &self.queues[@intFromEnum(dependency_info.priority)];
            if (queue.seq_consume >= dependency_info.seq) return;
            const dependency: *TransferInfo = &queue.elems[dependency_info.seq % queue.elems.len];
            if (dependency.cancel) return;
            self.complete(dependency) catch |err| {
                dependency.res.* = err;
            };
            if (queue.seq_consume == dependency_info.seq - 1)
                queue.seq_consume += 1
            else
                info.cancel = true;
        },
    }
}

fn runConsumer(self: *BlockDevice) noreturn {
    while (true) (iter: {
        const info = pop: {
            while (true) {
                for (&self.queues) |*queue| {
                    if (queue.pop()) |res| break :pop res;
                }
            }
        };
        // Info only modified on recursive call, error is already recorded by
        // .complete()
        self.complete(@constCast(&info)) catch {};
        scheduler.push(info.task) catch |err| break :iter err;
    }) catch |err| {
        console.print("Block device consumer error: {}\n", .{err});
        scheduler.terminate() catch std.debug.panic(
            "failed to terminate bad block device",
            .{},
        );
    };
}

fn consumerEntry() void {
    const self: *BlockDevice = @ptrCast(@alignCast(runway.dev));
    runway.mutex.release();
    runConsumer(self);
}
