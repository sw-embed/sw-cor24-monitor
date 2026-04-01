# Architecture: sw-cor24-monitor

## Memory Map

```
Address     Region              Description
─────────── ─────────────────── ──────────────────────────────
0x000000    Monitor code        Boot, invocation, registry, UART driver
0x000200    Monitor data        Program table, invocation block, buffers
0x000500    Service vector      Runtime/service table (putchar, getchar, etc.)
0x001000    Shell (sws)         sw-cor24-script loaded here (separate binary)
0x002000    Program slot 0      4K app slot (e.g., echo)
0x003000    Program slot 1      4K app slot
  ...         ...               Up to ~12 app slots in 4K regions
                                (or fewer apps in 32K reserved regions)
0x0FFFFF    End of SRAM         (1 MB total SRAM)
  ...
0xFEE000    EBR base            Embedded Block RAM (3 KB populated)
0xFEEC00    Stack top           Default SP (top of populated EBR)
  ...                           Single stack, grows downward (3K)
0xFF0000    I/O space           Memory-mapped I/O registers
0xFF0100    UART data           TX/RX data register
0xFF0101    UART status         Status: RX ready, TX busy, etc.
```

Stack: 3K in EBR, shared by monitor and current running program
(only one thing runs at a time). Can later expand to 8K EBR or
move to top of 1MB SRAM.

Loading: the emulator loads all binaries at specified addresses.
The monitor does not load programs — it only calls/runs them.

## Layer Diagram

```
┌──────────────────────────────────────────┐
│  Layer 4: Programs / Tools               │
│  (echo, failtest, sws tools, etc.)       │
│  Loaded at fixed addresses (0x2000+)     │
├──────────────────────────────────────────┤
│  Layer 3: Shell / Scripting (sws)        │
│  Loaded at 0x1000                        │
│  Uses monitor ABI to invoke programs     │
├──────────────────────────────────────────┤
│  Layer 2: Service Vector / Runtime       │
│  At 0x500: putchar, getchar, write,      │
│  readline, exit                          │
│  Programs call these, never UART MMIO    │
├──────────────────────────────────────────┤
│  Layer 1: Monitor / Service Processor    │
│  At 0x000: boot, UART HW driver,        │
│  program registry, invocation engine,    │
│  return trampoline, fault handler        │
└──────────────────────────────────────────┘
         │
         ▼
   ┌───────────┐
   │ UART MMIO │  (only monitor touches this)
   └───────────┘
```

## Service Vector Table Layout

The service vector at address 0x500 is an array of function entry
points. Programs receive a pointer to this table via the monitor
context, so they are not hard-coded to 0x500.

```
Offset  Service         Signature (conceptual)
──────  ──────────────  ──────────────────────────────
0       putchar         int putchar(int ch)
1       getchar         int getchar(void)
2       write           int write(char *buf, int len)
3       readline        int readline(char *buf, int max)
4       exit            void exit(int rc)
```

Each entry is one word (24-bit address of the service implementation).

## Invocation Block Structure

The monitor builds an invocation block before calling a program.
Since tc24r has no structs, this is a flat memory region at known
offsets:

```
Offset  Field       Description
──────  ──────────  ─────────────────────────────
+0      argc        Argument count
+1      argv        Pointer to argv array
+2      cmdline     Pointer to raw command tail string
+3      flags       Invocation flags (reserved)
```

The argv array is a sequence of pointers to null-terminated strings,
followed by a null sentinel.

## Program Registry Structure

The monitor maintains a program table using parallel arrays (no
structs in tc24r):

```
Array               Type        Description
──────────────────  ──────────  ─────────────────────────────
prog_names[N]       char*       Program name string pointer
prog_entries[N]     uint24      Entry point address
prog_bases[N]       uint24      Code region start
prog_ends[N]        uint24      Code region end
prog_flags[N]       int         Flags (callable, interactive, etc.)
prog_count          int         Number of registered programs
```

Programs are registered at boot time. The shell resolves names via
linear scan of `prog_names[]`.

## Boot Sequence

```
1. CPU reset → PC = 0x000000
2. Assembler bootstrap:
   a. Set monitor stack pointer (SP = 0xFEEC00)
   b. Jump to C main()
3. main():
   a. Initialize UART hardware (char * for byte-level I/O)
   b. Initialize service vector table
   c. Register programs (echo, ret42, exit7, etc.)
   d. Print boot banner ("cor24 monitor v0.4")
   e. Check if sws loaded at 0x1000 (non-zero word)
   f. If sws present: enter sws dispatch loop
   g. Otherwise: enter fallback built-in shell
```

## Execution Model

The monitor is the top-level controller. It calls sws (sw-cor24-script)
as its UI. When sws determines a program should run, it returns control
to the monitor with the program name. The monitor invokes the program.
When the program exits, the monitor calls sws again with the RC.

### sws Interface Protocol

sws communicates with the monitor via return codes and shared memory:

- **RC >= 256**: "run request" — sws wrote a program name to
  `mon_run_request[]` (32-byte shared buffer). Monitor runs the
  named program, stores the result in `mon_last_rc`, and calls
  sws again.
- **RC == 0**: sws quit normally. Monitor falls through to fallback
  shell or halts.
- **Other RC**: sws error. Monitor prints diagnostic and falls
  through to fallback shell.

```
Monitor sws loop:
  1. Call sws via mon_run(0x1000)
  2. If RC >= 256: extract name from mon_run_request
     a. Run named program via mon_run_by_name
     b. Store result RC in mon_last_rc
     c. Goto 1
  3. If RC == 0: sws exited, fall through
```

### Fallback Shell

If sws is not loaded at 0x1000, the monitor enters a built-in
minimal shell:

```
mon> <name>     — run a registered program
mon> list       — list registered programs
mon> help       — show available commands
```

After each program run, the shell prints `rc=N`.

sws lives in sw-cor24-script repo, compiled separately, loaded at
0x1000 by the emulator. It is developed in parallel and may not
have all features initially.

## Call Flow: Shell → Program → Return

```
Shell                    Monitor                  Program
─────                    ───────                  ───────
run echo hello ──────►  mon_run("echo", argv)
                         │
                         ├─ Resolve "echo" in registry
                         ├─ Build invocation block
                         ├─ Set up return trampoline
                         ├─ Push trampoline return addr
                         ├─ Call prog_main(ctx) ──────► Program entry
                         │                               │
                         │                               ├─ Read argc/argv
                         │                               ├─ ctx.svc.putchar()
                         │                               │     └──► Monitor UART write
                         │                               ├─ ctx.svc.getchar()
                         │                               │     └──► Monitor UART read
                         │                               │
                         │                               ├─ return rc
                         │                               │   OR
                         │                               ├─ ctx.svc.exit(rc)
                         │                               ▼
                         ◄── Return trampoline catches ──┘
                         │
                         ├─ Capture RC
                         ├─ Clean up invocation state
                         ├─ Return RC to caller
                         │
RC = result  ◄───────── return rc
```

## Error Flow

```
Normal:     prog returns rc  → monitor captures rc → shell gets rc
Exit call:  prog calls exit(rc) → monitor captures rc → shell gets rc
Fault:      prog crashes → trampoline/monitor detects → diagnostic
            → monitor returns fault RC → shell gets fault indicator
Not found:  mon_run("bad") → registry miss → monitor returns error RC
```
