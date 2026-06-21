const std = @import("std");
const builtin = @import("builtin");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const Target = std.Target.x86;
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .x86,
        .os_tag = .freestanding,
        .abi = .none,
        .cpu_features_add = Target.featureSet(&.{.soft_float}),
        .cpu_features_sub = Target.featureSet(&.{ .avx, .avx2, .sse, .sse2, .mmx }),
    });

    const mod = b.addModule("trampoline", .{
        .root_source_file = b.path("src/zig/trampoline.zig"),
        .target = target,
        .optimize = optimize,
        .code_model = .kernel,
    });
    switch (target.result.cpu.arch) {
        .x86_64 => mod.addAssemblyFile(b.path("boot64.S")),
        .x86 => mod.addAssemblyFile(b.path("boot32.S")),
        else => unreachable,
    }

    const exe = b.addExecutable(.{
        .name = "muse_trampoline",
        .root_module = mod,
    });
    exe.setLinkerScript(b.path("linker/trampoline.ld"));
    exe.use_llvm = true;
    exe.use_lld = true;

    b.installArtifact(exe);
}
