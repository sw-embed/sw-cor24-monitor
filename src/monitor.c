/* monitor.c — COR24 resident monitor
 * UART driver, service vector table, program invocation, shell, and boot init.
 */

char *IO_UARTDATA;
char *IO_UARTSTAT;

/* --- Program invocation globals (used by asm trampoline) --- */

int mon_saved_sp;
int mon_last_rc;

/* --- Program registry (parallel arrays, no structs) --- */

int prog_names[16];
int prog_entries[16];
int prog_flags[16];
int prog_count;

/* --- Shared memory for sws integration --- */

char mon_run_request[32];
int mon_sws_entry;

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

int mon_starts_with(char *str, char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str = str + 1;
        prefix = prefix + 1;
    }
    return 1;
}

int mon_strlen(char *s) {
    int n;
    n = 0;
    while (*s) {
        n = n + 1;
        s = s + 1;
    }
    return n;
}

void mon_strcpy(char *dst, char *src) {
    while (*src) {
        *dst = *src;
        dst = dst + 1;
        src = src + 1;
    }
    *dst = 0;
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

/* --- Fallback shell --- */

void mon_shell() {
    char line[64];
    int len;
    int rc;

    while (1) {
        uart_puts("mon> ");
        len = svc_readline(line, 64);

        if (len == 0) {
            continue;
        }

        if (mon_strcmp(line, "list") == 0) {
            mon_list_programs();
        } else if (mon_strcmp(line, "help") == 0) {
            uart_puts("commands:\n");
            uart_puts("  <name>  run program\n");
            uart_puts("  list    list programs\n");
            uart_puts("  help    show this help\n");
        } else {
            rc = mon_run_by_name(line);
            if (rc >= 0) {
                uart_puts("rc=");
                uart_put_int(rc);
                uart_putchar(10);
            }
        }
    }
}

/* --- sws integration --- */

int mon_sws_present() {
    int *addr;
    addr = 0x1000;
    return *addr != 0;
}

void mon_sws_loop() {
    int rc;

    while (1) {
        rc = mon_run(mon_sws_entry);
        mon_last_rc = rc;

        if (rc >= 256) {
            /* sws returned run request — name in mon_run_request */
            rc = mon_run_by_name(mon_run_request);
            mon_last_rc = rc;
        } else {
            /* sws exited normally (rc=0 quit, other = error) */
            uart_puts("sws exited, rc=");
            uart_put_int(rc);
            uart_putchar(10);
            return;
        }
    }
}

/* --- Boot --- */

int main() {
    uart_init();
    uart_puts("cor24 monitor v0.4\n");
    svc_init();

    /* Register programs */
    prog_count = 0;
    mon_register("echo", 0x2000, 0);
    mon_register("failtest", 0x3000, 0);
    mon_register("cat", 0x5000, 0);
    uart_puts("reg: ");
    uart_put_int(prog_count);
    uart_puts(" programs\n");

    /* sws shell or fallback */
    mon_sws_entry = 0x1000;
    if (mon_sws_present()) {
        uart_puts("sws: starting shell\n");
        mon_sws_loop();
    }

    /* Fallback to built-in shell */
    uart_puts("shell: fallback\n");
    mon_shell();

    return 0;
}
