const std = @import("std");
const modules = @import("modules.zig");
const scheduler = @import("scheduler.zig");
const cpu = @import("smp/cpu.zig");
const sync = @import("utils/sync.zig");
const console = @import("console.zig");
pub const Error = modules.InitError || error{IOFailed};

var runway: struct {
    dev: *anyopaque = undefined,
    mutex: sync.Mutex = .unlocked,
} = .{};

pub fn BlockDevice(dataType: type, sector_size: usize) type {
    return struct {
        const vfs = @import("vfs.zig");

        const TransferInfo = struct {
            bfrs: [][]u8,
            dir: vfs.Vnode.Direction,
            lba: u64,
            task: *scheduler.Task,
            res: *Error!void,
        };
        const TransferFunction = *const fn (
            data: dataType,
            bfrs: [][]u8,
            dir: vfs.Vnode.Direction,
            lba: u64,
        ) Error!void;

        data: dataType,
        transferRaw: TransferFunction,
        queue: struct {
            elems: []TransferInfo,
            pushMutex: sync.Mutex,
            producer: std.atomic.Value(usize),
            consumer: usize,
        },

        const CreationError = std.mem.Allocator.Error || modules.InitError || error{FileAlreadyExists};
        pub fn new(
            data: dataType,
            transferRaw: @This().TransferFunction,
            queueLen: ?usize,
            gpa: std.mem.Allocator,
            name: []const u8,
        ) CreationError!void {
            const default_queue_len = 64;

            const self = try gpa.create(@This());
            self.* = .{
                .data = data,
                .transferRaw = transferRaw,
                .queue = .{
                    .elems = try gpa.alloc(TransferInfo, queueLen orelse default_queue_len),
                    .pushMutex = .unlocked,
                    .producer = .init(0),
                    .consumer = 0,
                },
            };
            const root = try vfs.mod.dataRef();
            const dev = root.data.directory.children.getPtr("dev") orelse unreachable;
            const block_inode: vfs.Vnode = .{
                .payload = @ptrCast(self),
                .data = .{ .file = .{
                    .transfer = &@This().transfer,
                } },
            };
            try dev.data.directory.children.put(name, block_inode);
            const inode = dev.data.directory.children.getPtr(name) orelse unreachable;
            const contextSwitch = @import("arch.zig").contextSwitch;
            try runway.mutex.acquire();
            errdefer runway.mutex.release();
            runway.dev = self;
            const task = contextSwitch.createKernelTask(
                &@This().consumerEntry,
                gpa,
            ) catch return error.CriticalSystemFailure;
            try scheduler.push(task);

            var meow: [512]u8 = @splat(0);
            block_inode.data.file.transfer(
                inode,
                &meow,
                .read,
                0,
            ) catch unreachable;
            block_inode.data.file.transfer(
                inode,
                &meow,
                .read,
                1,
            ) catch unreachable;
        }

        fn transfer(
            vnode: *vfs.Vnode,
            data: []u8,
            dir: vfs.Vnode.Direction,
            offset: usize,
        ) vfs.Vnode.TransferError!void {
            const self: *@This() = @ptrCast(@alignCast(vnode.payload));
            return self.transferQueued(data, dir, offset);
        }

        fn transferQueued(
            self: *@This(),
            data: []u8,
            dir_p: vfs.Vnode.Direction,
            offset: usize,
        ) vfs.Vnode.TransferError!void {
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
            const use_post = post_end > post_start and post_start != pre_start;

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

                    self.produceTransfer(
                        bfrs,
                        .read,
                        @intCast(lba_start),
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
                },
                .write => std.debug.panic("Block device write unsupported", .{}),
            }
        }

        fn produceTransfer(
            self: *@This(),
            bfrs: [][]u8,
            dir: vfs.Vnode.Direction,
            lba: u64,
        ) Error!void {
            try self.queue.pushMutex.acquire();
            defer self.queue.pushMutex.release();
            while ((self.queue.consumer + 1) % self.queue.elems.len == self.queue.producer.load(.monotonic))
                try scheduler.preempt();
            var res: Error!void = {};
            self.queue.elems[self.queue.producer.raw] = .{
                .bfrs = bfrs,
                .dir = dir,
                .lba = lba,
                .task = (try cpu.getActiveCPU()).queue.active,
                .res = &res,
            };
            const new_idx = (self.queue.producer.raw + 1) % self.queue.elems.len;
            self.queue.producer.store(new_idx, .release);
            console.print("Produced transfer, waiting...\n", .{});
            try scheduler.schedule();
            console.print("Done!\n", .{});
            return res;
        }

        fn pop(self: *@This()) modules.InitError!TransferInfo {
            while (self.queue.producer.load(.acquire) == self.queue.consumer)
                try scheduler.preempt();
            const idx = self.queue.consumer;
            self.queue.consumer = (idx + 1) % self.queue.elems.len;
            return self.queue.elems[idx];
        }

        fn runConsumer(self: *@This()) noreturn {
            while (true) (iter: {
                const info = self.pop() catch |err| break :iter err;
                console.print("Recieved transfer, executing...\n", .{});
                self.transferRaw(
                    self.data,
                    info.bfrs,
                    info.dir,
                    info.lba,
                ) catch |err| {
                    info.res.* = err;
                };
                scheduler.push(info.task) catch |err| break :iter err;
            }) catch |err| {
                console.print("block device consumer error: {}\n", .{err});
                scheduler.terminate() catch std.debug.panic(
                    "failed to terminate bad block device",
                    .{},
                );
            };
        }

        fn consumerEntry() void {
            const self: *@This() = @ptrCast(@alignCast(runway.dev));
            runway.mutex.release();
            runConsumer(self);
        }
    };
}
