const std = @import("std");
const builtin = @import("builtin");

fn fatMkdir(b: *std.Build, esp: std.Build.LazyPath, comptime path: []const u8) *std.Build.Step.Run {
    const step = b.addSystemCommand(&.{"mmd"});
    step.addArgs(&.{"-i"});
    step.addFileArg(esp);
    step.addArg("::" ++ path);
    return step;
}

fn fatCpy(b: *std.Build, esp: std.Build.LazyPath, src: std.Build.LazyPath, comptime path: []const u8) *std.Build.Step.Run {
    const step = b.addSystemCommand(&.{"mcopy"});
    step.addArgs(&.{"-i"});
    step.addFileArg(esp);
    step.addFileArg(src);
    step.addArg("::" ++ path);
    step.addFileInput(src);
    return step;
}

const TargetSpecificInfo = struct {
    bootstrap_asm: std.Build.LazyPath,
    efi: []const u8,
    grub_arch: []const u8,
    grub_path: []const u8,
    vm: []const u8,
    bios: []const u8,
};

pub fn build(b: *std.Build) !void {
    const optimize = b.standardOptimizeOption(.{});
    const Target = std.Target.x86;
    const target = b.resolveTargetQuery(.{
        .cpu_arch = b.option(std.Target.Cpu.Arch, "arch", "CPU architecture to compile the kernel for") orelse .x86_64,
        .os_tag = .freestanding,
        .abi = .none,
        .cpu_features_add = Target.featureSet(&.{.soft_float}),
        .cpu_features_sub = Target.featureSet(&.{ .avx, .avx2, .sse, .sse2, .mmx }),
    });

    const info: TargetSpecificInfo = switch (target.result.cpu.arch) {
        .x86 => .{
            .bootstrap_asm = b.path("boot32.S"),
            .efi = "artifacts/BOOTIA32.EFI",
            .grub_arch = "i386-efi",
            .grub_path = "/lib/grub/i386-efi",
            .vm = "qemu-system-i386",
            .bios = "if=pflash,format=raw,readonly=on,file=deps/bios32.bin",
        },
        .x86_64 => .{
            .bootstrap_asm = b.path("boot64.S"),
            .efi = "artifacts/BOOTX64.EFI",
            .grub_arch = "x86_64-efi",
            .grub_path = "/home/ashy5000/etc/grub-build/grub-core",
            .vm = "qemu-system-x86_64",
            .bios = "if=pflash,format=raw,readonly=on,file=deps/bios64.bin",
        },
        else => unreachable,
    };

    const mod = b.addModule("trampoline", .{
        .root_source_file = b.path("src/zig/trampoline.zig"),
        .target = target,
        .optimize = optimize,
        .red_zone = false,
        // .code_model = .kernel,
    });
    mod.addAssemblyFile(info.bootstrap_asm);

    const font_path = b.option(
        []const u8,
        "font_path",
        "path to a PSF console font",
    ) orelse "font.psf";
    mod.addAnonymousImport("font", .{
        .root_source_file = b.graph.cwdRelativePath(font_path),
    });

    const exe = b.addExecutable(.{
        .name = "muse_trampoline",
        .root_module = mod,
    });
    exe.setLinkerScript(b.path("linker/trampoline.ld"));
    exe.use_llvm = true;
    exe.use_lld = true;

    const wf = b.addWriteFiles();
    const efi = wf.add(info.efi, &.{});

    const grub_dir = b.option(
        []const u8,
        "grub_path",
        "Path to a grub build directory",
    );
    const grub_step = b.addSystemCommand(&.{if (grub_dir) |dir|
        try std.mem.concat(b.allocator, u8, &.{ dir, "/grub-mkstandalone" })
    else
        "grub-mkstandalone"});
    if (grub_dir) |dir| {
        grub_step.addArgs(&.{
            "-d",
            try std.mem.concat(b.allocator, u8, &.{ dir, "/grub-core/" }),
        });
    }
    grub_step.addArg("-O");
    grub_step.addArg(info.grub_arch);
    grub_step.addArg("-o");
    grub_step.addFileArg(efi);
    grub_step.addArg("boot/grub/grub.cfg=grub/grub_mem.cfg");

    const esp = wf.add("artifacts/esp.img", &.{});

    const esp_truncate_step = b.addSystemCommand(&.{"truncate"});
    esp_truncate_step.addArgs(&.{ "-s", "64M" });
    esp_truncate_step.addFileArg(esp);

    const esp_mkfs_step = b.addSystemCommand(&.{"mkfs.fat"});
    esp_mkfs_step.addArgs(&.{"-F32"});
    esp_mkfs_step.addFileArg(esp);
    esp_mkfs_step.step.dependOn(&esp_truncate_step.step);

    const mmd_step_0 = fatMkdir(b, esp, "/EFI");
    mmd_step_0.step.dependOn(&esp_mkfs_step.step);

    const mmd_step_1 = fatMkdir(b, esp, "/EFI/BOOT");
    mmd_step_1.step.dependOn(&esp_mkfs_step.step);
    mmd_step_1.step.dependOn(&mmd_step_0.step);

    const mmd_step_2 = fatMkdir(b, esp, "/boot");
    mmd_step_2.step.dependOn(&esp_mkfs_step.step);

    const mmd_step_3 = fatMkdir(b, esp, "/boot/grub");
    mmd_step_3.step.dependOn(&esp_mkfs_step.step);
    mmd_step_3.step.dependOn(&mmd_step_2.step);

    const mcopy_step_0 = fatCpy(b, esp, efi, "/EFI/BOOT/");
    mcopy_step_0.step.dependOn(&mmd_step_1.step);
    mcopy_step_0.step.dependOn(&grub_step.step);

    const mcopy_step_1 = fatCpy(b, esp, b.path("grub/grub_disk.cfg"), "/boot/grub");
    mcopy_step_1.step.dependOn(&mmd_step_3.step);

    const mcopy_step_2 = fatCpy(b, esp, exe.getEmittedBin(), "/boot/muse");
    mcopy_step_2.step.dependOn(&mmd_step_2.step);
    mcopy_step_2.step.dependOn(b.getInstallStep());

    const esp_step = b.addInstallFileWithDir(esp, .prefix, "esp.img");
    esp_step.step.dependOn(&mcopy_step_0.step);
    esp_step.step.dependOn(&mcopy_step_1.step);
    esp_step.step.dependOn(&mcopy_step_2.step);

    const disk = wf.add("artifacts/disk.img", &.{});

    const disk_truncate_step = b.addSystemCommand(&.{"truncate"});
    disk_truncate_step.addArgs(&.{ "-s", "128M" });
    disk_truncate_step.addFileArg(disk);

    const zap_step = b.addSystemCommand(&.{"sgdisk"});
    zap_step.addFileArg(disk);
    zap_step.addArgs(&.{"-Z"});
    zap_step.step.dependOn(&disk_truncate_step.step);

    const sgdisk_step = b.addSystemCommand(&.{"sgdisk"});
    sgdisk_step.addFileArg(disk);
    sgdisk_step.addArgs(&.{
        "-n",
        "1:2048:+64M",
        "-t",
        "1:ef00",
        "-n",
        "2:0:0",
        "-t",
        "2:8300",
    });
    sgdisk_step.step.dependOn(&zap_step.step);

    const dd_step = b.addSystemCommand(&.{"dd"});
    dd_step.addArgs(&.{
        "bs=512",
        "seek=2048",
        "conv=notrunc",
    });
    dd_step.addPrefixedFileArg("if=", esp);
    dd_step.addPrefixedFileArg("of=", disk);
    dd_step.step.dependOn(&sgdisk_step.step);
    dd_step.step.dependOn(&esp_step.step);

    const img_step = b.step("img", "Build a bootable disk image with muse");
    img_step.dependOn(&dd_step.step);

    const qemu_step = b.addSystemCommand(&.{info.vm});
    qemu_step.addArgs(&.{
        "-device",
        "nvme,serial=deadbeef,drive=nvm",
        "-drive",
    });
    qemu_step.addPrefixedFileArg("format=raw,id=nvm,if=none,file=", disk);
    qemu_step.addArg("-drive");

    const firmware_path = b.option(
        []const u8,
        "firmware_path",
        "path to UEFI firmware for QEMU",
    ) orelse {
        std.debug.print("Missing -Dfirmware_path.\n", .{});
        return error.NoFirmware;
    };

    qemu_step.addArg(try std.mem.concat(
        b.allocator,
        u8,
        &.{ "if=pflash,format=raw,readonly=on,file=", firmware_path },
    ));
    qemu_step.addArgs(&.{
        "-no-reboot",
        "-no-shutdown",
        // "-debugcon",
        // "stdio",
        "-d",
        "int,trace:ide*",
        "-smp",
        "2",
    });

    const debug = b.option(bool, "debug", "Make QEMU wait for a GDB connection") orelse false;
    if (debug) {
        qemu_step.addArgs(&.{
            "-s",
            "-S",
        });
    }

    qemu_step.step.dependOn(img_step);

    const run_step = b.step("run", "Run muse in QEMU");
    run_step.dependOn(&qemu_step.step);

    b.installArtifact(exe);

    const subsystems_mod = b.addModule("subsystems", .{
        .root_source_file = b.path("tools/subsystems.zig"),
        .target = b.standardTargetOptions(.{}),
    });
    const subsystems = b.addExecutable(.{
        .name = "subsystems",
        .root_module = subsystems_mod,
    });
    const subsystems_run = b.addRunArtifact(subsystems);
    const generated_drivers_file = subsystems_run.addOutputFileArg("drivers.zig");
    const write_drivers = b.addUpdateSourceFiles();
    write_drivers.addCopyFileToSource(generated_drivers_file, "src/zig/drivers.zig");
    const update_drivers = b.step("update-drivers", "Configure and update src/drivers/drivers.zig");
    update_drivers.dependOn(&write_drivers.step);
}
