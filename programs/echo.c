/* echo.c — test program for monitor invocation
 * Receives ctx (service vector pointer), prints "OK" via putchar, returns 0.
 */

int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    vec = ctx;
    putchar_fn = vec[0];    /* slot 0 = svc_putchar */
    putchar_fn(79);         /* 'O' */
    putchar_fn(75);         /* 'K' */
    putchar_fn(10);         /* newline */
    return 0;
}
