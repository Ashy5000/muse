const modules = @import("modules.zig");
const console = @import("console.zig");

pub var cpu_vendor_id: ?[12]u8 = null;

fn getCPUVendorID() void {
    var fst: u32 = undefined;
    var snd: u32 = undefined;
    var lst: u32 = undefined;
    asm (
        \\ mov $0, %eax
        \\ cpuid
        : [fst] "={ebx}" (fst),
          [snd] "={edx}" (snd),
          [lst] "={ecx}" (lst),
        :: .{ .eax = true }
    );
    const res_32: [3]u32 = .{ fst, snd, lst };
    cpu_vendor_id = @bitCast(res_32);
}

pub const CPUFeatures = packed struct {
    // EBX
    brand_idx: u8,
    cflush_line_size: u8,
    apic_id_space: u8,
    initial_apic_id: u8,
    // ECX
    sse3: bool,
    pclmul: bool,
    dtes64: bool,
    monitor: bool,
    ds_cpl: bool,
    vmx: bool,
    smx: bool,
    est: bool,
    tm2: bool,
    ssse3: bool,
    cid: bool,
    sdbg: bool,
    fma: bool,
    cx16: bool,
    xtpr: bool,
    pdcm: bool,
    rsvd0: bool,
    pcid: bool,
    dca: bool,
    sse4_1: bool,
    sse4_2: bool,
    x2apic: bool,
    movbe: bool,
    popcnt: bool,
    ecx_tsc: bool,
    aes: bool,
    xsave: bool,
    osxsave: bool,
    avx: bool,
    f16c: bool,
    rdrand: bool,
    hypervisor: bool,
    // EDX
    fpu: bool,
    vme: bool,
    de: bool,
    pse: bool,
    edx_tsc: bool,
    msr: bool,
    pae: bool,
    mce: bool,
    cx8: bool,
    apic: bool,
    rsvd1: bool,
    sep: bool,
    mtrr: bool,
    pge: bool,
    mca: bool,
    cmov: bool,
    pat: bool,
    pse36: bool,
    psn: bool,
    clflush: bool,
    rsvd2: bool,
    ds: bool,
    acpi: bool,
    mmx: bool,
    fxsr: bool,
    sse: bool,
    sse2: bool,
    ss: bool,
    htt: bool,
    tm: bool,
    ia64: bool,
    pbe: bool,
};

pub var cpu_features: ?CPUFeatures = null;

fn getCPUFeatures() void {
    var ebx: u32 = undefined;
    var ecx: u32 = undefined;
    var edx: u32 = undefined;
    asm (
        \\ mov $1, %eax
        \\ cpuid
        : [ebx] "={ebx}" (ebx),
          [ecx] "={ecx}" (ecx),
          [edx] "={edx}" (edx),
        :: .{ .eax = true }
    );
    const concat: [3]u32 = .{ ebx, ecx, edx };
    cpu_features = @bitCast(concat);
}

pub const CPUExtendedInfo = packed struct {
    // ECX
    lahf_lm: bool,
    cmp_legacy: bool,
    svm: bool,
    extapic: bool,
    cr8_legacy: bool,
    abm_lzcnt: bool,
    sse4a: bool,
    misalignsse: bool,
    @"3dnow_prefetch": bool,
    osvw: bool,
    ibs: bool,
    xop: bool,
    skinit: bool,
    wdt: bool,
    rsvd0: bool,
    lwp: bool,
    fma4: bool,
    tce: bool,
    rsvd1: bool,
    nodeid_msr: bool,
    rsvd2: bool,
    tbm: bool,
    topoext: bool,
    perfctr_core: bool,
    perfctr_nb: bool,
    stream_perf_mon: bool,
    dbx: bool,
    perftsc: bool,
    pcx_l: bool,
    monitorx: bool,
    addr_mask_ext: bool,
    rsvd3: bool,
    // EDX
    fpu: bool,
    vme: bool,
    de: bool,
    pse: bool,
    edx_tsc: bool,
    msr: bool,
    pae: bool,
    mce: bool,
    cx8: bool,
    apic: bool,
    syscall_k6: bool,
    syscall: bool,
    mtrr: bool,
    pge: bool,
    mca: bool,
    cmov: bool,
    pat: bool,
    pse36: bool,
    ecc_k7: bool,
    ecc: bool,
    nx: bool,
    rsvd4: bool,
    mmxext: bool,
    mmx: bool,
    fxsr: bool,
    fxsr_opt: bool,
    pdpe1gb: bool,
    rdtscp: bool,
    rex32_k8: bool,
    lm: bool,
    @"3dnownext": bool,
    @"3dnow": bool,
};

pub var cpu_extended_info: ?CPUExtendedInfo = null;

fn getCPUExtendedInfo() void {
    var ecx: u32 = undefined;
    var edx: u32 = undefined;
    asm (
        \\ mov $0x80000001, %eax
        \\ cpuid
        : [ecx] "={ecx}" (ecx),
          [edx] "={edx}" (edx),
        :: .{ .eax = true }
    );
    const concat: [2]u32 = .{ ecx, edx };
    cpu_extended_info = @bitCast(concat);
}
fn init() modules.ModuleInitError!void {
    getCPUVendorID();
    getCPUFeatures();
    getCPUExtendedInfo();
    console.print("CPU vendor ID: {s}.\n", .{cpu_vendor_id.?});
}

pub var mod: modules.Module = .{
    .name = "cpuid",
    .init = init,
};
