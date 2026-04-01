# sw-cor24-monitor — Claude Instructions

## Project Overview

This is a COR24 resident monitor / service processor / terminal
controller. It boots at address 0, owns the UART hardware exclusively,
and provides a stable program-invocation ABI with service-vector-based
I/O for loaded programs. It uses synchronous call/return execution
(no scheduler/multitasking). Programs are invoked with arguments and
return an integer RC. The monitor is the environment where the sws
scripting language (sw-cor24-script) is developed and dogfooded.

Future vision: the monitor acts as a front-end terminal multiplexor
for multiple FPGA/MCU CPUs running their own RTOS instances, but the
near-term goal is single-CPU synchronous program invocation.

## CRITICAL: AgentRail Session Protocol (MUST follow exactly)

### 1. START (do this FIRST, before anything else)
```bash
agentrail next
```
Read the output carefully. It contains your current step, prompt,
plan context, and any relevant skills/trajectories.

### 2. BEGIN (immediately after reading the next output)
```bash
agentrail begin
```

### 3. WORK (do what the step prompt says)
Do NOT ask "want me to proceed?". The step prompt IS your instruction.
Execute it directly.

### 4. COMMIT (after the work is done)
Commit your code changes with git. Use `/mw-cp` for the checkpoint
process (pre-commit checks, docs, detailed commit, push).
**Run `/mw-cp` in every repo that was modified during the step.**

### 5. COMPLETE (LAST thing, after committing)
```bash
agentrail complete --summary "what you accomplished" \
  --reward 1 \
  --actions "tools and approach used"
```
- If the step failed: `--reward -1 --failure-mode "what went wrong"`
- If the saga is finished: add `--done`

### 6. STOP (after complete, DO NOT continue working)
Do NOT make further code changes after running `agentrail complete`.
Any changes after complete are untracked and invisible to the next
session. Future work belongs in the NEXT step, not this one.

## Key Rules

- **Do NOT skip steps** — the next session depends on accurate tracking
- **Do NOT ask for permission** — the step prompt is the instruction
- **Do NOT continue working** after `agentrail complete`
- **Commit before complete** — always commit first, then record completion

## Useful Commands

```bash
agentrail status          # Current saga state
agentrail history         # All completed steps
agentrail plan            # View the plan
agentrail next            # Current step + context
```

## Build / Test

The monitor is written in a mix of COR24 assembler and C (tc24r subset).
- Assembler: bootstrap, context switch, entry/return trampoline
- C: monitor logic, program registry, UART services, shell integration
- Build: `tc24r` (cross C compiler from sw-cor24-x-tinyc) +
  `sw-cor24-x-assembler` (cross assembler in Rust)
- Build orchestration: justfiles with assembly linker for relocation
- Run: `cor24-run` (emulator) loads multiple binaries at specified
  addresses (monitor at 0, sws at 0x1000, apps at 0x2000+)

## tc24r C Subset Constraints

- No structs — use parallel arrays or packed flat memory
- No malloc/free — static allocation only
- No string library — implement helpers manually
- No varargs, single translation unit
- 24-bit int, 8-bit char
- UART I/O only (memory-mapped)

## Cross-Repo Context

All COR24 repos live under `~/github/sw-embed/` as siblings:
- `sw-cor24-emulator` — emulator + ISA (foundation)
- `sw-cor24-x-assembler` — cross-assembler in Rust (runs on host)
- `sw-cor24-x-tinyc` — cross C compiler in Rust (runs on host)
- `sw-cor24-assembler` — native assembler in C (runs on COR24)
- `sw-cor24-script` — sws scripting language (runs on COR24, uses monitor)
- `sw-cor24-project` — hub/portal repo with migration tracking
- `web-sw-cor24-assembler` — web IDE

You are the only agent running for this project and have direct r/w
access to all sw-embed repos.
