# Product Requirements Document: sw-cor24-monitor

## Problem Statement

The COR24 soft CPU ecosystem has cross-compilers, cross-assemblers,
interpreters, and an emulator, but lacks a resident environment that
can invoke programs with arguments and collect return codes. Programs
currently must own UART hardware directly, making it impossible to
share a terminal or build higher-level tooling like a scripting
language that orchestrates multiple programs.

## Goals

1. Provide a stable, synchronous program-invocation ABI for COR24
2. Centralize UART hardware ownership in one place (the monitor)
3. Enable the sws scripting language to invoke programs, pass args,
   and inspect return codes
4. Establish a service-vector-based I/O API that decouples programs
   from hardware details
5. Create a foundation that can later evolve into a terminal
   multiplexor for multi-CPU FPGA/MCU systems

## Target Users

- **sw-cor24-script (sws)**: primary consumer; needs `run prog args`
  with RC propagation
- **COR24 program authors**: C and assembler programs that need
  portable I/O without touching UART MMIO
- **Educational audience**: the system should be understandable on
  one whiteboard

## Use Cases

1. **Interactive shell session**: user types commands at a prompt;
   monitor invokes programs and returns to prompt on completion
2. **Scripted invocation**: sws script runs a program, checks `$rc`,
   branches on success/failure
3. **Program I/O**: loaded program calls `putchar`, `getchar`,
   `write`, `readline` through service vector without knowing
   UART hardware addresses
4. **Program registry**: shell says `run echo hello` and monitor
   resolves "echo" to its entry address and invokes it
5. **Fault recovery**: if a program crashes, monitor reports a
   diagnostic and returns control to the shell

## Functional Requirements

### FR-1: Monitor Boot
- Monitor image resides at address 0
- On reset: initialize UART, initialize monitor data structures,
  register built-in programs, invoke the shell

### FR-2: Program Invocation
- `mon_run(name, argv)` resolves a program name, prepares an
  invocation block (argc, argv, cmdline, flags), transfers control
  to the program entry point, and captures the return code
- Programs enter via `int prog_main(mon_context *ctx)` where ctx
  provides both the invocation block and service vector pointers
- Return trampoline ensures monitor always regains control

### FR-3: UART Ownership
- Only the monitor touches UART MMIO registers
- Programs use service vector calls: putchar, getchar, write, readline
- Synchronous blocking I/O is acceptable for the initial version

### FR-4: Return Code Handling
- Program RC (from return or exit call) propagates to the caller
- Shell stores RC as `$rc` for scripting use
- Three error classes distinguished:
  - Program RC (0 = success, nonzero = app-defined)
  - Monitor invocation failure (no such program, bad args)
  - Program fault (bad jump, stack corruption)

### FR-5: Program Registry
- Monitor maintains a table mapping program names to entry addresses
- Registry entries include: name, entry address, base, end, flags
- Shell and scripts reference programs by name, never by address

### FR-6: Service Vector Table
- Fixed set of monitor services exposed as a vector table
- Initial services: putchar, getchar, write, readline, exit
- Programs receive a pointer to the service vector via the context

### FR-7: Argument Passing (future phase)
- Initially: no argument passing; programs receive only service
  vector pointer
- Future: invocation block with argc, argv, cmdline, flags
- Monitor will build the invocation block from the shell/script
  command when args are added

### FR-8: Fault Handling
- If a program faults (bad return, stack corruption, invalid service
  call), monitor prints a diagnostic and returns control to shell
- Fault information is available for inspection (fault PC, reason)

## Non-Functional Requirements

### NF-1: tc24r C Subset Constraints
- No structs: use parallel arrays or packed flat memory at known
  offsets
- No malloc/free: all allocation is static
- No string library: implement helpers manually
- No varargs, single translation unit
- 24-bit int, 8-bit char

### NF-2: Memory Constraints
- All code and data fit in COR24 address space
- Fixed memory map with known regions for monitor, runtime, shell,
  and program slots
- Static allocation limits must be reasonable and documented

### NF-3: Educational Clarity
- Prefer visible, explicit mechanisms over clever tricks
- Fixed addresses, explicit state, clear control flow
- The system should be explainable on one whiteboard

### NF-4: Portability
- Must run on cor24-run emulator
- Must be buildable with tc24r (cross C compiler) and as24 (cross
  assembler)
- Future: run on FPGA soft CPU

## Out of Scope (for now)

- Multitasking, cooperative scheduling, preemption
- Background jobs or concurrent tasks
- Dynamic program loading from filesystem
- Memory protection or privilege separation
- Pipes, signals, or IPC
- Multi-CPU terminal multiplexing (future goal, not initial scope)
- Complex buffering or per-client I/O queues
- Timer interrupts or sleep primitives
