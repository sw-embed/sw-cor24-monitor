/* exit7.c — test svc_exit: calls svc_exit(7) through service vector */

int main(int ctx) {
    int *vec;
    int (*exit_fn)();
    vec = ctx;
    exit_fn = vec[4];      /* slot 4 = svc_exit */
    exit_fn(7);
    /* should not reach here */
    return 99;
}
