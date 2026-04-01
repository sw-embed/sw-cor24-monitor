/* failtest.c — demo program: prints error message and returns nonzero RC
 * Demonstrates nonzero return code handling.
 */

int main(int ctx) {
    int *vec;
    int (*putchar_fn)();
    char *msg;
    vec = ctx;
    putchar_fn = vec[0];    /* slot 0 = svc_putchar */
    msg = "something went wrong\n";
    while (*msg) {
        putchar_fn(*msg);
        msg = msg + 1;
    }
    return 12;
}
