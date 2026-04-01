# Implementation Plan: sw-cor24-monitor

## Phase Overview

```
Phase 1: Bootstrap & UART         ← foundation
Phase 2: Service Vector & Runtime ← program-facing API
Phase 3: Program Invocation       ← mon_run + trampoline + RC
Phase 4: Program Registry         ← name resolution, program table
Phase 5: sws Shell Integration    ← monitor ↔ sws loop, $rc
Phase 6: Demo & Dogfooding        ← echo, failtest, end-to-end
```

## Design Decisions (from user feedback)

- **No args initially**: phase 1 passes only service vector pointer,
  argc/argv added later
- **3K stack in EBR** (0xFEE000-0xFEEC00), shared by monitor and
  current program (one thing at a time)
- **Programs in 4K slots** (0x2000, 0x3000, ...), could use 32K
  regions for larger apps; ~12 slots
- **No software interrupts**: service calls use direct function
  call through vector table
- **Emulator loads everything**: monitor does not load programs,
  just calls/runs preloaded binaries
- **sws is the UI**: monitor calls sw-cor24-script as its shell;
  sws is a separate repo/binary loaded at 0x1000
- **sws is WIP**: developed in parallel, may need a tiny fallback
  loop in the monitor until sws is ready
- **Build**: tc24r → assembler → binary; justfiles orchestrate;
  emulator loads binaries at specified addresses

## Phase Dependencies

```
Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4 ──► Phase 5 ──► Phase 6
  boot        svc API     call/ret    registry     sws loop     demo
  UART        putchar     trampoline  name lookup  $rc flow     echo
              getchar     exit svc    prog table   fallback     failtest
```

Each phase builds on the previous. No phase can be skipped.

---

## Phase 1: Bootstrap & UART Driver

**Goal**: Monitor boots at address 0, initializes UART, can print
a banner and read characters.

**Tasks**:
1. Assembler boot stub: set stack pointer (0xFEEC00), jump to C
   `monitor_init()`
2. UART driver: `uart_init()`, `uart_putchar()`, `uart_getchar()`,
   `uart_puts()` using MMIO at 0xFF0100/0xFF0101
3. `monitor_init()`: init UART, print boot banner
4. Build script (justfile): tc24r → assembler → binary
5. Verify on cor24-run emulator

**Testing**:
- Boot → see banner on UART output
- `uart_putchar` / `uart_getchar` work correctly
- Emulator run completes without crash

**Deliverables**: boot.s, monitor.c (partial), justfile

---

## Phase 2: Service Vector & Runtime

**Goal**: Service vector table at 0x500 populated with I/O services.
A test program can call services through the vector.

**Tasks**:
1. Define service vector layout (flat array of function addresses)
2. Implement service functions: `svc_putchar`, `svc_getchar`,
   `svc_write`, `svc_readline`, `svc_exit`
3. Initialize vector table in `monitor_init()`
4. Write a minimal test: hard-coded program that uses vector to print

**Testing**:
- Service vector entries resolve to correct functions
- Calling putchar through vector produces UART output
- Exit service returns control to monitor

**Deliverables**: service vector init code, service implementations

---

## Phase 3: Program Invocation (mon_run + Trampoline)

**Goal**: Monitor can invoke a program at a known address with a
service vector pointer, and regain control when it returns or
calls exit. No argument passing yet.

**Tasks**:
1. Implement `mon_run(entry)` in C — sets up context (service
   vector pointer only), calls assembler trampoline
2. Assembler return trampoline: pushes return stub address, pushes
   ctx pointer as arg, jumps to program entry
3. Return stub: captures RC from r0, restores monitor stack,
   returns to mon_run
4. Exit service: routes through same completion path
5. Context: single-word pointer to service vector table

**Testing**:
- Hard-coded program at 0x2000 that prints "hello" and returns 0
- mon_run(0x2000) → program runs → RC = 0 captured
- Program that calls exit(42) → RC = 42 captured
- Program that just returns 7 → RC = 7 captured

**Deliverables**: mon_run(), trampoline.s, context setup

---

## Phase 4: Program Registry

**Goal**: Programs are registered by name. Monitor can resolve a
name to an entry address.

**Tasks**:
1. Program registry: parallel arrays (names, entries, flags),
   registration function
2. Name lookup: `mon_find_program(name)` → index or -1
3. `mon_run_by_name(name)` → find + invoke
4. Register demo programs at boot time
5. String helper: `mon_strcmp()`

**Testing**:
- Register "echo" at 0x2000, look it up by name → found
- Look up "nonexistent" → not found
- mon_run_by_name("echo") invokes correctly
- Multiple programs registered and callable

**Deliverables**: registry code, string helpers

---

## Phase 5: sws Shell Integration

**Goal**: Monitor calls sws (sw-cor24-script) as its UI in a loop.
sws tells the monitor which program to run. Monitor runs it,
captures RC, calls sws again.

**Tasks**:
1. Define monitor ↔ sws interface: how sws returns a program
   name to the monitor, how monitor passes RC back
2. Monitor main loop: call sws → get program name → run program →
   store RC → call sws again
3. Tiny fallback shell: if sws is not loaded at 0x1000, provide a
   minimal `mon>` prompt with `run <name>` command
4. RC propagation: monitor stores last RC in a known location,
   sws can read it
5. Error reporting: "program not found", "invocation failed"

**Testing**:
- Monitor calls sws, sws returns "echo", monitor runs echo
- After echo exits, monitor calls sws again with RC
- Fallback shell works when sws is not present
- RC is visible to sws as `$rc`

**Deliverables**: monitor main loop, sws interface, fallback shell

---

## Phase 6: Demo & Dogfooding

**Goal**: End-to-end validation with real demo programs.

**Tasks**:
1. Demo: `echo` — prints a fixed message and returns 0
2. Demo: `failtest` — returns a nonzero RC
3. Demo: `cat` or `type` — echoes stdin to stdout until 'q'
4. Integration test: monitor boots → sws runs → invokes programs
   → RC flows back
5. Document the program author's guide (how to write a program
   for the monitor)

**Testing**:
- Full demo flow:
  ```
  sws> run echo
  hello world
  rc=0
  sws> run failtest
  something went wrong
  rc=12
  ```
- Multiple programs invoked in sequence
- Verify no state leakage between invocations

**Deliverables**: demo programs, integration tests, author guide

---

## Milestone Summary

### Milestone 1: Synchronous Monitored Program Call (Phases 1-4)

Validates: boot, UART, service vector, invocation, trampoline,
registry, RC capture.

Demo: monitor invokes a preloaded program, captures RC.

### Milestone 2: sws Integration & Dogfooding (Phases 5-6)

Validates: monitor ↔ sws loop, program invocation from script,
RC propagation, error handling.

Demo: sws scripting language runs programs and checks return codes.

This is the dogfooding point where sws development proceeds
with the monitor as its runtime environment.
