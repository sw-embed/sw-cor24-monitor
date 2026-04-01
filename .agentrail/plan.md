# COR24 Monitor

Resident monitor at address 0 that owns UART, provides service vector
for I/O, and invokes preloaded programs synchronously with RC capture.

## Constraints
- tc24r C subset: no structs, no malloc, no string lib, single TU
- 24-bit int, 8-bit char, UART MMIO at 0xFF0100/0xFF0101
- 3K stack in EBR (SP = 0xFEEC00), one program at a time
- Programs in 4K slots at 0x2000+, sws shell at 0x1000
- Emulator loads all binaries; monitor just calls/runs them
- No args initially; just service vector pointer via context

## Phases
1. Bootstrap & UART driver (boot.s + monitor.c + justfile)
2. Service vector table (I/O services at 0x500)
3. Program invocation (mon_run + return trampoline + RC capture)
4. Program registry (name → entry lookup, parallel arrays)
5. sws shell integration (monitor ↔ sws loop, fallback shell)
6. Demo & dogfooding (echo, failtest, end-to-end validation)

## Key ABIs
- Registers: r0 (retval), r1 (retaddr), r2, fp, sp, z, iv, ir
- Calling: args on stack R-to-L, jal r1, return via jmp (r1)
- Prologue: push fp/r2/r1, mov fp,sp
- Service vector: array of function pointers at 0x500
- Program entry: int prog_main(ctx) where ctx → service vector
