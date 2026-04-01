/* monitor.c — COR24 resident monitor
 * UART driver and boot initialization.
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

int main() {
    uart_init();
    uart_puts("cor24 monitor v0.1\n");
    return 0;
}
