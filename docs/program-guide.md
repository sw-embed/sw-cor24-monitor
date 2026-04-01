# COR24 Monitor — Program Guide

How to write, compile, and run programs under the COR24 monitor.

## Program Entry Convention

Programs are compiled with `tc24r` and loaded by the emulator at a
fixed slot address. The monitor invokes programs via `mon_run()`,
which calls a trampoline that:

1. Saves the monitor's stack pointer
2. Pushes `ctx` (service vector pointer) as the program's argument
3. Jumps to the program entry point

Your program's `main` receives one argument:

```c
int main(int ctx) {
    int *vec;
    vec = ctx;       /* cast to pointer to service vector */
    /* ... use services via vec[N] ... */
    return 0;        /* return code */
}
```

The return value of `main` becomes the program's return code (RC).
The monitor prints `rc=N` after each invocation.

## Service Vector

The `ctx` argument points to an array of function pointers (the
service vector table). Programs call monitor services by loading
function pointers from this array:

| Slot | Function | Signature | Description |
|------|----------|-----------|-------------|
| 0 | svc_putchar | `int putchar(int ch)` | Write one character to UART |
| 1 | svc_getchar | `int getchar()` | Read one character from UART (blocking) |
| 2 | svc_write | `int write(char *buf, int len)` | Write buffer to UART |
| 3 | svc_readline | `int readline(char *buf, int max)` | Read line with echo and backspace |
| 4 | svc_exit | `void exit(int rc)` | Exit immediately with RC (noreturn) |

### Calling a service

```c
int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    vec = ctx;
    putchar_fn = vec[0];    /* slot 0 = svc_putchar */
    putchar_fn(65);          /* prints 'A' */
    return 0;
}
```

### svc_exit vs return

Programs can exit two ways:
- **`return N`** from `main` — normal return, RC = N
- **`svc_exit(N)`** via slot 4 — immediate exit from any depth, RC = N

Both paths return control to the monitor cleanly.

## Memory Layout

| Address | Contents |
|---------|----------|
| 0x0000 | Monitor code + data |
| 0x0500 | Service vector table (5 entries) |
| 0x1000 | sws shell (if present) |
| 0x2000 | Program slot 1 (echo) |
| 0x3000 | Program slot 2 (failtest) |
| 0x4000 | Program slot 3 |
| 0x5000 | Program slot 4 (cat) |
| 0xFEEC00 | Stack top (3K EBR) |

Programs must fit within their slot (4K = 0x1000 bytes). The stack
is shared — the monitor saves/restores SP across invocations.

## Compiling with tc24r

tc24r is a C subset compiler for COR24. Constraints:
- No structs — use parallel arrays or packed memory
- No malloc/free — static allocation only
- No standard library — use service vector for I/O
- 24-bit int, 8-bit char
- Single translation unit

Build a program:

```bash
just build-prog programs/myprogram.c 0x6000
```

This compiles with tc24r, strips the default `_start`, prepends
`prog_start.s` (which forwards ctx to main and returns RC), and
assembles at the specified base address.

## Running Programs

### Interactive (terminal mode)

```bash
just run
```

This starts the monitor with `--terminal` for interactive use.
Type program names at the `mon>` prompt.

### Non-interactive (testing)

```bash
just test
```

Runs with a 1-second time limit. Use `--uart-input` (`-u`) to
send commands:

```bash
cor24-run ... -u 'echo\nfailtest\n'
```

### Shell Commands

| Command | Description |
|---------|-------------|
| `<name>` | Run a registered program |
| `list` | List all registered programs |
| `help` | Show available commands |

## Return Code Convention

- **0** — success
- **nonzero** — error (program-defined meaning)

The monitor prints `rc=N` after each program exits and stores the
last RC in `mon_last_rc` for the sws shell to read.

## Example Programs

### echo.c — Hello World

```c
int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    char *msg;
    vec = ctx;
    putchar_fn = vec[0];
    msg = "hello world\n";
    while (*msg) {
        putchar_fn(*msg);
        msg = msg + 1;
    }
    return 0;
}
```

### failtest.c — Nonzero RC

```c
int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    char *msg;
    vec = ctx;
    putchar_fn = vec[0];
    msg = "something went wrong\n";
    while (*msg) {
        putchar_fn(*msg);
        msg = msg + 1;
    }
    return 12;
}
```

### cat.c — Interactive I/O

```c
int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    int (*getchar_fn)();
    int ch;
    vec = ctx;
    putchar_fn = vec[0];
    getchar_fn = vec[1];
    ch = getchar_fn();
    while (ch != 113) {     /* 'q' to quit */
        putchar_fn(ch);
        ch = getchar_fn();
    }
    return 0;
}
```
