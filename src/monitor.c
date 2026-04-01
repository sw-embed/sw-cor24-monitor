/* monitor.c — COR24 resident monitor
 * UART driver, service vector table, and boot initialization.
 */

int *IO_UARTDATA;
int *IO_UARTSTAT;

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

/* --- Service vector table (compiler-placed, address passed via context) --- */

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
    /* Stub — will be implemented in phase 3 (program invocation).
     * For now, print message and halt. */
    uart_puts("svc_exit: halt\n");
    while (1) {}
    return rc;
}

void svc_init() {
    svc_vector[0] = svc_putchar;
    svc_vector[1] = svc_getchar;
    svc_vector[2] = svc_write;
    svc_vector[3] = svc_readline;
    svc_vector[4] = svc_exit;
}

/* --- Validation: call putchar through the service vector --- */

void svc_test() {
    svc_vector[0](79);  /* 'O' */
    svc_vector[0](75);  /* 'K' */
    svc_vector[0](10);  /* newline */
}

int main() {
    uart_init();
    uart_puts("cor24 monitor v0.1\n");
    svc_init();
    uart_puts("svc: vector ready\n");
    svc_test();
    return 0;
}
