/* recover.S — exception recovery trampoline for KDOS command_kern.
 *
 * exc_save() saves callee-saved regs + return addr + RSP into _exc_buf.
 * Returns 0 to its caller (normal path).
 *
 * exc_restore is the IRETQ target.  exception_handler writes its address
 * into the IRETQ frame (f->rip = exc_restore, f->rsp = exc_restore_rsp).
 * When IRETQ fires, exc_restore restores the saved state and "returns" 1
 * to whoever called exc_save() — appearing as a second return of exc_save.
 *
 * Buffer layout (_exc_buf, 8 × uint64):
 *   [0] rip  — return address of exc_save() call (= resume point in caller)
 *   [1] rsp  — caller's RSP after exc_save() returns (rsp_at_call + 8)
 *   [2] rbp
 *   [3] rbx
 *   [4] r12
 *   [5] r13
 *   [6] r14
 *   [7] r15
 */

    .section .bss
    .align 8
_exc_buf:
    .space 64

    .section .text

/* ── exc_save ─────────────────────────────────────────────── */

    .global exc_save
    .type   exc_save, @function
exc_save:
    movq  (%rsp), %rax                  /* return address */
    leaq  _exc_buf(%rip), %rcx
    movq  %rax,   0(%rcx)               /* [0] = rip */
    leaq  8(%rsp), %rax                 /* RSP as it will be after ret */
    movq  %rax,   8(%rcx)               /* [1] = rsp */
    movq  %rbp,  16(%rcx)               /* [2] = rbp */
    movq  %rbx,  24(%rcx)               /* [3] = rbx */
    movq  %r12,  32(%rcx)               /* [4] = r12 */
    movq  %r13,  40(%rcx)               /* [5] = r13 */
    movq  %r14,  48(%rcx)               /* [6] = r14 */
    movq  %r15,  56(%rcx)               /* [7] = r15 */
    xorl  %eax, %eax                    /* return 0 */
    ret

/* ── exc_restore (IRETQ target) ───────────────────────────── */

    .global exc_restore
    .type   exc_restore, @function
exc_restore:
    /* On entry: RSP = exc_restore_rsp (valid but transient).
       Restore full callee state and jump to the saved resume point,
       returning 1 to look like a second return of exc_save(). */
    leaq  _exc_buf(%rip), %rax
    movq   8(%rax), %rsp                /* [1] restore rsp */
    movq  16(%rax), %rbp                /* [2] restore rbp */
    movq  24(%rax), %rbx                /* [3] restore rbx */
    movq  32(%rax), %r12                /* [4] restore r12 */
    movq  40(%rax), %r13                /* [5] restore r13 */
    movq  48(%rax), %r14                /* [6] restore r14 */
    movq  56(%rax), %r15                /* [7] restore r15 */
    movq   0(%rax), %rcx                /* [0] resume rip */
    movl  $1, %eax                      /* return value = 1 */
    jmp   *%rcx
