/* cat.c — demo program: echoes input characters until 'q'
 * Demonstrates interactive I/O through the service vector.
 */

int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    int (*getchar_fn)();
    int ch;
    vec = ctx;
    putchar_fn = vec[0];    /* slot 0 = svc_putchar */
    getchar_fn = vec[1];    /* slot 1 = svc_getchar */
    ch = getchar_fn();
    while (ch != 113) {      /* 113 = 'q' */
        putchar_fn(ch);
        ch = getchar_fn();
    }
    return 0;
}
