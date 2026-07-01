const console = @import("../../console.zig");

/// Creates and prints a register dump for an x86 system.
pub fn dumpRegs() void {
    var eax: u32 = undefined;
    var ebx: u32 = undefined;
    var ecx: u32 = undefined;
    var edx: u32 = undefined;
    var esi: u32 = undefined;
    var edi: u32 = undefined;
    var ebp: u32 = undefined;
    var esp: u32 = undefined;
    asm (
        \\ mov %%eax, %[eax]
        \\ mov %%ebx, %[ebx]
        \\ mov %%ecx, %[ecx]
        \\ mov %%edx, %[edx]
        \\ mov %%esi, %[esi]
        \\ mov %%edi, %[edi]
        \\ mov %%ebp, %[ebp]
        \\ mov %%esp, %[esp]
        : [eax] "=m" (eax),
          [ebx] "=m" (ebx),
          [ecx] "=m" (ecx),
          [edx] "=m" (edx),
          [esi] "=m" (esi),
          [edi] "=m" (edi),
          [ebp] "=m" (ebp),
          [esp] "=m" (esp),
    );
    console.print("EAX=0x{x:0>8} EBX=0x{x:0>8} ECX=0x{x:0>8} EDX=0x{x:0>8}\n", .{ eax, ebx, ecx, edx });
    console.print("ESI=0x{x:0>8} EDI=0x{x:0>8}\n", .{ esi, edi });
    console.print("Stack frame: EBP=0x{x:0>8} -> ESP=0x{x:0>8}\n", .{ ebp, esp });
    if (esp > ebp) {
        console.print("WARN: ESP > EBP\n", .{});
    }
}
