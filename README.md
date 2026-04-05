# sw-cor24-monitor

Resident monitor and service processor for the COR24 soft CPU.

## Overview

The monitor boots at address 0, owns the UART hardware exclusively,
and provides a stable program-invocation ABI with service-vector-based
I/O for loaded programs. It uses synchronous call/return execution —
programs are invoked by name and return an integer RC.

The monitor calls [sw-cor24-script](../sw-cor24-script) (sws) as its
interactive shell/UI. When sws requests a program to run, the monitor
invokes it, captures the return code, and passes control back to sws.

## Architecture

```
Monitor (0x000) → sws shell (0x20000) → programs (0x40000+)
    │                                        │
    └── UART MMIO (0xFF0100) ◄───────────────┘
        (only monitor touches HW)       (via service vector)
```

- **Service vector** at 0x500: putchar, getchar, write, readline, exit
- **Programs** receive a context pointer to the service vector
- **RC propagation**: program RC flows back to shell as `$rc`

## Demos

```bash
just demo-editor          # monitor -> sws -> swye editor (filtered output)
just demo-editor-dump     # same, with full memory hex dump
```

The editor demo boots the monitor, launches sws via staged UART input,
runs the yocto-ed editor against a preloaded text buffer with preloaded
keystrokes, and displays the edited result back in sws.

## Status

In development.

## Related Repositories

| Repo | Description |
|------|-------------|
| [sw-cor24-emulator](../sw-cor24-emulator) | Emulator + ISA (runs the monitor) |
| [sw-cor24-x-tinyc](../sw-cor24-x-tinyc) | Cross C compiler (`tc24r`) |
| [sw-cor24-x-assembler](../sw-cor24-x-assembler) | Cross assembler (Rust) |
| [sw-cor24-script](../sw-cor24-script) | sws scripting language (shell/UI) |
| [sw-cor24-macrolisp](../sw-cor24-macrolisp) | MacroLisp interpreter (app) |
| [sw-cor24-forth](../sw-cor24-forth) | Forth interpreter (app) |
| [sw-cor24-assembler](../sw-cor24-assembler) | Native assembler (app) |
| [sw-cor24-project](../sw-cor24-project) | Hub/portal with migration tracking |

## License

See repository license file.
