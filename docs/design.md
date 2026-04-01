# Design: sw-cor24-monitor

## ABI Specifications

### Program Entry Convention

Every callable program exports a single entry point with this
logical signature:

```
int prog_main(mon_context *ctx)
```

Where `ctx` points to a flat memory block containing:
- Pointer to the invocation block (argc, argv, cmdline, flags)
- Pointer to the service vector table

Since tc24r has no structs, `ctx` is a pointer to a known-offset
flat region:

```
ctx + 0  →  pointer to service vector table
ctx + 1  →  (reserved for future invocation block / argc / argv)
```

**Phase 1 simplification**: no argument passing yet. Programs
receive only the service vector pointer via ctx. Argument passing
(argc, argv, cmdline) will be added in a later phase.

The program receives `ctx` as its first argument (pushed on stack
per tc24r convention).

Return value: integer RC in r0.

### Service Call Convention

Programs call monitor services through the service vector table.
The table is an array of function pointers at a known address.

To call `putchar(ch)`:
1. Load service vector base from ctx
2. Load entry 0 (putchar address) from vector
3. Place `ch` in argument register
4. Call the loaded address
5. Return value in return register

This indirection means programs never hard-code monitor internal
addresses. The monitor can relocate service implementations freely.

### Calling Convention (Registers/Stack)

COR24 has 8 registers (3-bit encoding):

```
Reg  Name   Purpose
───  ─────  ──────────────────────────────
r0   r0     Return value / scratch
r1   r1     Return address (jal saves here) / scratch
r2   r2     Scratch / callee-saved
r3   fp     Frame pointer (callee-saved)
r4   sp     Stack pointer
r5   z/c    Zero register / condition flag
r6   iv     Interrupt vector
r7   ir     Interrupt return address
```

tc24r calling convention (from compiler source):
- **Arguments**: pushed on stack right-to-left (all args on stack)
- **Call**: `la r0,target; jal r1,(r0)` — saves return address in r1
- **Return value**: in r0
- **Prologue**: `push fp; push r2; push r1; mov fp,sp` then
  allocate locals
- **Epilogue**: `mov sp,fp; pop r1; pop r2; pop fp; jmp (r1)`
- **Callee-saved**: fp, r2, r1 (saved/restored by prologue/epilogue)
- **Caller-saved**: r0 (used for return value and call target)
- **Stack cleanup**: caller adds `N*3` to sp after call (N = arg count,
  3 bytes per word)
- **Stack grows downward**
- **Default SP**: 0xFEEC00 (top of EBR populated area)

The monitor trampoline must save/restore fp, r2, r1 when setting
up program invocation and restoring monitor context.

## UART Ownership Model

**Core rule**: only the monitor touches UART MMIO. Everything else
calls monitor services.

### Write Path

```
Program calls ctx.svc.putchar(ch)
  → Monitor service handler
    → Write ch to UART TX register (MMIO)
    → Spin/poll until TX ready (synchronous)
    → Return to program
```

For `write(buf, len)`: loop calling putchar internally.

Write is synchronous and blocking. No output buffering in v1.

### Read Path

```
Program calls ctx.svc.getchar()
  → Monitor service handler
    → Poll UART RX register (MMIO)
    → Block until char available (spin-wait)
    → Return char to program
```

For `readline(buf, max)`: read chars into buffer until newline or
max reached. Optional line editing (backspace) in monitor.

Read is synchronous and blocking. Acceptable because we have no
multitasking — only one program runs at a time.

### UART MMIO Addresses

From sw-cor24-emulator `cpu/state.rs`:

```
Address     Register      Description
──────────  ────────────  ─────────────────────────────────
0xFF0100    IO_UARTDATA   Write: transmit byte. Read: receive byte
                          (auto-acknowledges RX on read)
0xFF0101    IO_UARTSTAT   Status register:
                            bit 0: RX data ready
                            bit 1: CTS active
                            bit 2: RX overflow
                            bit 7: TX busy
```

Other I/O:
- 0xFF0000 IO_LEDSWDAT — LED/switch (bit 0)
- 0xFF0010 IO_INTENABLE — interrupt enable (bit 0 = UART RX)

Memory regions:
- 0x000000-0x0FFFFF: SRAM (1 MB)
- 0xFEE000-0xFEFFFF: EBR (8 KB window, 3 KB populated)
- 0xFF0000-0xFFFFFF: I/O space

## Return Trampoline Mechanism

The trampoline ensures the monitor always regains control when a
program finishes, regardless of how it exits.

### Setup (before program entry)

```asm
; Push return trampoline address onto stack
; so that when prog_main returns, it "returns" to the trampoline
push mon_return_stub
; Push ctx pointer as argument
push ctx
; Jump to program entry
jump prog_entry
```

### Trampoline stub (assembler)

```
mon_return_stub:
    ; RC is in return register (r0/r1)
    ; Store RC into monitor's result location
    store rc, [mon_last_rc]
    ; Restore monitor stack pointer
    load sp, [mon_saved_sp]
    ; Return to mon_run caller
    jump mon_run_complete
```

### Exit service path

```
svc_exit(rc):
    ; Store RC
    store rc, [mon_last_rc]
    ; Restore monitor stack
    load sp, [mon_saved_sp]
    ; Same completion path
    jump mon_run_complete
```

Both paths converge at `mon_run_complete`, which restores monitor
state and returns the RC to the caller (shell).

## Error Handling

### Class 1: Program RC

Normal program completion. The program returned an integer:
- 0 = success (by convention)
- Nonzero = application-defined error

The monitor stores this as the invocation result. Shell reads it
as `$rc`.

### Class 2: Monitor Invocation Failure

The monitor could not invoke the program:
- Program name not found in registry
- Invalid entry address
- Bad argument block

The monitor returns a reserved error RC (e.g., -1 or 0xFFFFFF)
and sets a diagnostic flag. Shell can distinguish this from a
program RC via a separate status variable or reserved RC range.

### Class 3: Program Fault

The program misbehaved:
- Returned to unexpected address (caught by trampoline)
- Called an invalid service vector index
- Stack corruption detected

The monitor prints a diagnostic message to UART, stores fault
information (fault PC, reason code), and returns a fault RC.

### Diagnostic State

The monitor maintains:
- `mon_last_rc`: last program return code
- `mon_last_error`: last monitor error (0 = none)
- `mon_last_fault_pc`: PC at fault (if applicable)
- `mon_last_fault_reason`: reason code

## Assembler vs C Code Split

### In Assembler (as24)

- **Reset/boot entry**: CPU starts at address 0, must set up stack
  and jump to C code
- **Return trampoline**: `mon_return_stub` — precise stack/register
  control needed
- **Service vector veneers**: optional thin wrappers if calling
  convention requires register shuffling
- **Context setup for program entry**: pushing trampoline address
  and arguments before jumping to program

### In C (tc24r)

- **monitor_init()**: UART init, data init, program registration,
  boot banner, shell invocation
- **mon_run()**: name resolution, invocation block building, RC
  capture and return
- **Program registry**: parallel arrays, lookup by name
- **String helpers**: strcmp, strlen, strcpy, argument parsing
- **Shell integration**: command parsing, `$rc` propagation
- **UART service implementations**: putchar, getchar, write,
  readline (the actual I/O logic)
- **Diagnostics**: fault reporting, error messages

### Rationale

Assembler is used only where precise machine control is required
(boot, trampoline, context manipulation). Everything else is in C
for readability and faster iteration. The C compiler (tc24r) handles
the calling convention automatically for regular function calls.

## tc24r Constraints and Workarounds

### No Structs → Parallel Arrays

Instead of:
```c
struct program { char *name; int entry; int flags; };
struct program table[8];
```

Use:
```c
int prog_names[8];    /* pointers to name strings */
int prog_entries[8];  /* entry addresses */
int prog_flags[8];    /* flag words */
int prog_count;
```

Access pattern: `prog_names[i]`, `prog_entries[i]`, `prog_flags[i]`
always refer to the same logical program.

### No Structs → Flat Memory Blocks

For the invocation block and context, use known offsets into a
flat memory region:

```c
int invocation[4];  /* [0]=argc, [1]=argv, [2]=cmdline, [3]=flags */
int context[2];     /* [0]=&invocation, [1]=&service_vector */
```

### No malloc → Static Allocation

All buffers, tables, and strings are statically allocated:
```c
int prog_names[8];
int prog_entries[8];
char line_buf[128];
char arg_buf[256];
int argv_ptrs[16];
```

Limits are compile-time constants.

### No String Library → Manual Helpers

Implement: `mon_strcmp`, `mon_strlen`, `mon_strcpy`, `mon_atoi`
as needed. Keep them minimal.

### Single Translation Unit

All C code compiles as one file (or is `#include`-concatenated).
This is a tc24r requirement.

### 24-bit int, 8-bit char

Pointers and ints are 24-bit. Chars are 8-bit. String operations
must account for this. Array indexing uses 24-bit arithmetic.

## Build Workflow

1. `tc24r` compiles C source to COR24 assembly
2. Assembly linker (in justfiles) relocates code to target addresses
3. `sw-cor24-x-assembler` (not `as24`) assembles to binary
4. Emulator loads multiple binaries at specified addresses:
   - monitor.bin at 0x000000
   - script.bin at 0x001000
   - app1.bin at 0x002000
   - etc.
5. `cor24-run` executes from address 0

Build scripts use justfiles. The assembler is `sw-cor24-x-assembler`
(cross assembler in Rust, runs on host).
