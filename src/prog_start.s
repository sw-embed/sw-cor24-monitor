; prog_start.s — startup for monitor-loaded programs
; Replaces tc24r's default _start so programs return to the monitor
; trampoline instead of halting.
;
; Entry: ctx on stack (pushed by trampoline), r1 = return address
; Exit:  r0 = main(ctx) return value, returns to trampoline

        .text

        .globl  _start
_start:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; Forward ctx argument to main
        lw      r0,9(fp)
        push    r0
        la      r0,_main
        jal     r1,(r0)
        add     sp,3
        ; r0 = RC from main
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)
