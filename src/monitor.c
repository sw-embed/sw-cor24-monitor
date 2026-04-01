/* monitor.c — COR24 resident monitor
 * UART driver, service vector table, program invocation, and boot init.
 */

int *IO_UARTDATA;
int *IO_UARTSTAT;

/* --- Program invocation globals (used by asm trampoline) --- */

int mon_saved_sp;
int mon_last_rc;

/* --- Program registry (parallel arrays, no structs) --- */

int prog_names[16];
int prog_entries[16];
int prog_flags[16];
int prog_count;

void uart_init() {
    IO_UARTDATA = 0xFF0100;
    IO_UARTSTAT = 0xFF0101;
}

void uart_putchar(int ch) {
    int stat;
    stat = *IO_UARTSTAT;
    while (stat & 128) {
        stat = *IO_UARTSTAT;
    }
    *IO_UARTDATA = ch;
}

int uart_getchar() {
    int stat;
    stat = *IO_UARTSTAT;
    while (!(stat & 1)) {
        stat = *IO_UARTSTAT;
    }
    return *IO_UARTDATA;
}

void uart_puts(char *s) {
    while (*s) {
        uart_putchar(*s);
        s = s + 1;
    }
}

void uart_put_int(int n) {
    int d;
    int started;
    int div;
    if (n < 0) {
        uart_putchar(45);
        n = 0 - n;
    }
    if (n == 0) {
        uart_putchar(48);
        return;
    }
    div = 100000;
    started = 0;
    while (div > 0) {
        d = n / div;
        if (d > 0 || started) {
            uart_putchar(48 + d);
            started = 1;
        }
        n = n - d * div;
        div = div / 10;
    }
}

/* --- Service vector table --- */

int (*svc_vector[5])();

int svc_putchar(int ch) {
    uart_putchar(ch);
    return 0;
}

int svc_getchar() {
    return uart_getchar();
}

int svc_write(char *buf, int len) {
    int i;
    i = 0;
    while (i < len) {
        uart_putchar(buf[i]);
        i = i + 1;
    }
    return len;
}

int svc_readline(char *buf, int max) {
    int ch;
    int i;
    int done;
    i = 0;
    done = 0;
    while (i < max - 1 && !done) {
        ch = uart_getchar();
        if (ch == 10 || ch == 13) {
            uart_putchar(10);
            done = 1;
        } else if (ch == 8 || ch == 127) {
            if (i > 0) {
                i = i - 1;
                uart_putchar(8);
                uart_putchar(32);
                uart_putchar(8);
            }
        } else {
            buf[i] = ch;
            uart_putchar(ch);
            i = i + 1;
        }
    }
    buf[i] = 0;
    return i;
}

int svc_exit(int rc) {
    /* Placeholder — overwritten by svc_set_exit() with asm impl.
     * If somehow called before wiring, halt. */
    uart_puts("svc_exit: not wired\n");
    while (1) {}
    return rc;
}

void svc_init() {
    svc_vector[0] = svc_putchar;
    svc_vector[1] = svc_getchar;
    svc_vector[2] = svc_write;
    svc_vector[3] = svc_readline;
    svc_vector[4] = svc_exit;
    /* svc_set_exit() overwrites slot 4 with asm svc_exit_impl */
    svc_set_exit();
}

/* --- String helpers --- */

int mon_strcmp(char *a, char *b) {
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        a = a + 1;
        b = b + 1;
    }
    return *a - *b;
}

/* --- Program registry --- */

int mon_register(char *name, int entry, int flags) {
    int i;
    i = prog_count;
    if (i >= 16) return -1;
    prog_names[i] = name;
    prog_entries[i] = entry;
    prog_flags[i] = flags;
    prog_count = i + 1;
    return i;
}

int mon_find_program(char *name) {
    int i;
    i = 0;
    while (i < prog_count) {
        if (mon_strcmp(name, prog_names[i]) == 0) return i;
        i = i + 1;
    }
    return -1;
}

int mon_run_by_name(char *name) {
    int idx;
    idx = mon_find_program(name);
    if (idx < 0) {
        uart_puts("unknown program: ");
        uart_puts(name);
        uart_putchar(10);
        return -1;
    }
    return mon_run(prog_entries[idx]);
}

void mon_list_programs() {
    int i;
    i = 0;
    while (i < prog_count) {
        uart_puts(prog_names[i]);
        uart_putchar(10);
        i = i + 1;
    }
}

/* --- Program invocation --- */

int mon_run(int entry) {
    int rc;
    int ctx;
    ctx = svc_vector;
    rc = mon_invoke_program(entry, ctx);
    return rc;
}

/* --- Boot --- */

int main() {
    int rc;
    uart_init();
    uart_puts("cor24 monitor v0.3\n");
    svc_init();
    uart_puts("svc: vector ready\n");

    /* Register demo programs */
    prog_count = 0;
    mon_register("echo", 0x2000, 0);
    mon_register("ret42", 0x3000, 0);
    mon_register("exit7", 0x4000, 0);
    uart_puts("reg: ");
    uart_put_int(prog_count);
    uart_puts(" programs\n");

    /* Test: list programs */
    uart_puts("--- list ---\n");
    mon_list_programs();

    /* Test: find by name */
    uart_puts("find echo=");
    uart_put_int(mon_find_program("echo"));
    uart_putchar(10);
    uart_puts("find nope=");
    uart_put_int(mon_find_program("nope"));
    uart_putchar(10);

    /* Test: run by name */
    uart_puts("--- run echo ---\n");
    rc = mon_run_by_name("echo");
    uart_puts("rc=");
    uart_put_int(rc);
    uart_putchar(10);

    uart_puts("--- run ret42 ---\n");
    rc = mon_run_by_name("ret42");
    uart_puts("rc=");
    uart_put_int(rc);
    uart_putchar(10);

    uart_puts("--- run exit7 ---\n");
    rc = mon_run_by_name("exit7");
    uart_puts("rc=");
    uart_put_int(rc);
    uart_putchar(10);

    /* Test: run unknown program */
    uart_puts("--- run bogus ---\n");
    rc = mon_run_by_name("bogus");
    uart_puts("rc=");
    uart_put_int(rc);
    uart_putchar(10);

    return 0;
}
