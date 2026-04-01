; boot.s — COR24 monitor bootstrap
; Sets up stack pointer, then falls through to tc24r _start.

        la      r0,0xFEEC00         ; load stack address
        mov     sp,r0               ; stack pointer = top of 3K EBR
; tc24r _start follows immediately (concatenated)
