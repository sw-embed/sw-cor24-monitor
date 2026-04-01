; trampoline.s — program invocation trampoline and svc_exit
;
; mon_invoke_program(entry, ctx): call program, return RC in r0
; svc_exit_impl(rc): non-local return to mon_invoke_program's caller
; svc_set_exit(): wire svc_exit_impl into service vector slot 4

        .text

; ---------------------------------------------------------------
; mon_invoke_program(entry, ctx) -> int rc
;   Called from C.  Saves monitor SP, pushes ctx as the program's
;   argument, jumps to entry.  On normal return, RC is in r0.
; ---------------------------------------------------------------
        .globl  _mon_invoke_program
_mon_invoke_program:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; Save monitor SP so svc_exit_impl can unwind here
        mov     r0,sp
        la      r2,_mon_saved_sp
        sw      r0,0(r2)
        ; Load entry address (arg1)
        lw      r2,9(fp)
        ; Load ctx (arg2) and push as program's first argument
        lw      r0,12(fp)
        push    r0
        ; Call program entry — r1 gets return address
        jal     r1,(r2)
        ; Program returned normally, RC in r0
        add     sp,3
        ; Epilogue — return RC to mon_run
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)

; ---------------------------------------------------------------
; svc_exit_impl(rc) -> noreturn
;   Called by programs through the service vector (slot 4).
;   Restores the monitor SP saved by mon_invoke_program and
;   performs its epilogue, effectively longjmp-ing back to the
;   mon_run caller with rc in r0.
; ---------------------------------------------------------------
        .globl  _svc_exit_impl
_svc_exit_impl:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; Get rc argument, stash in mon_last_rc
        lw      r0,9(fp)
        la      r2,_mon_last_rc
        sw      r0,0(r2)
        ; Restore monitor SP (saved in mon_invoke_program)
        la      r0,_mon_saved_sp
        lw      r0,0(r0)
        mov     sp,r0
        ; SP now points to mon_invoke_program's frame —
        ; do its epilogue so we return to mon_run
        pop     r1
        pop     r2
        pop     fp
        ; Reload rc into r0 (return value)
        la      r0,_mon_last_rc
        lw      r0,0(r0)
        jmp     (r1)

; ---------------------------------------------------------------
; svc_set_exit() -> void
;   Writes address of _svc_exit_impl into svc_vector[4].
;   Called from C after svc_init() to wire up the exit service.
; ---------------------------------------------------------------
        .globl  _svc_set_exit
_svc_set_exit:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        la      r0,_svc_exit_impl
        la      r1,_svc_vector
        ; slot 4 at offset 12 (4 * 3 bytes per word)
        add     r1,12
        sw      r0,0(r1)
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)
