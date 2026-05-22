/* ============================================================
   ISR STUBS — x86_64
   Exceptions without an error code get a dummy 0 pushed first
   so every vector arrives at isr_common_stub with the same
   stack layout:  ... | error_code | vector | <GP regs> ...
   ============================================================ */

.section .text
.code64

/* ---- Helpers ------------------------------------------------ */
.macro ISR_NOERR num
.global isr\num
isr\num:
    pushq $0          /* dummy error code */
    pushq $\num
    jmp   isr_common_stub
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    /* CPU already pushed the real error code */
    pushq $\num
    jmp   isr_common_stub
.endm

/* ---- CPU exception vectors 0–31 ---------------------------- */
ISR_NOERR  0    /* #DE  Division Error                   */
ISR_NOERR  1    /* #DB  Debug                            */
ISR_NOERR  2    /*      NMI                              */
ISR_NOERR  3    /* #BP  Breakpoint                       */
ISR_NOERR  4    /* #OF  Overflow                         */
ISR_NOERR  5    /* #BR  Bound Range Exceeded             */
ISR_NOERR  6    /* #UD  Invalid Opcode                   */
ISR_NOERR  7    /* #NM  Device Not Available             */
ISR_ERR    8    /* #DF  Double Fault                     */
ISR_NOERR  9    /*      Coprocessor Segment Overrun      */
ISR_ERR   10    /* #TS  Invalid TSS                      */
ISR_ERR   11    /* #NP  Segment Not Present              */
ISR_ERR   12    /* #SS  Stack-Segment Fault              */
ISR_ERR   13    /* #GP  General Protection Fault         */
ISR_ERR   14    /* #PF  Page Fault                       */
ISR_NOERR 15    /*      Reserved                         */
ISR_NOERR 16    /* #MF  x87 Floating-Point Exception     */
ISR_ERR   17    /* #AC  Alignment Check                  */
ISR_NOERR 18    /* #MC  Machine Check                    */
ISR_NOERR 19    /* #XF  SIMD Floating-Point Exception    */
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30    /* #SX  Security Exception               */
ISR_NOERR 31    /*      Reserved                         */

/* ---- Common stub ------------------------------------------- */
/* On entry the stack holds (top → bottom):
     vector, error_code, rip, cs, rflags, rsp, ss           */
isr_common_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq  %rsp, %rdi          /* first arg: pointer to interrupt_frame_t */
    call  exception_handler

    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %r11
    popq  %r10
    popq  %r9
    popq  %r8
    popq  %rdi
    popq  %rsi
    popq  %rbp
    popq  %rdx
    popq  %rcx
    popq  %rbx
    popq  %rax

    addq  $16, %rsp           /* discard vector + error_code */
    iretq
