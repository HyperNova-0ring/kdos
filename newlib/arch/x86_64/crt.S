/*
 * crt.S — x86_64 module CRT startup.
 *
 * The kernel calls: _module_entry(const kst_t* kst)
 *   kst arrives in %rdi per the SysV x86_64 ABI.
 *
 * This stub:
 *   1. Saves kst (%rdi will be clobbered by BSS zeroing).
 *   2. Zeros the BSS region so newlib's internal globals start clean.
 *   3. Stores kst in _kst (used by the newlib hooks in newlib.c).
 *   4. Aligns the stack to the 16-byte SysV requirement.
 *   5. Calls int main(int argc, char** argv) with argc=0, argv=NULL.
 *   6. Calls _exit() with main's return value — never returns.
 */

    .extern _kst            /* const kst_t* — defined in newlib.c */
    .extern _impure_ptr     /* struct _reent* — defined in newlib.c */
    .extern _impure_data    /* struct _reent  — defined in newlib.c */
    .extern main            /* int main(int, char**) — defined by the module */
    .extern _exit           /* void _exit(int) — defined in newlib.c */
    .extern __bss_start     /* linker symbol: start of BSS */
    .extern __bss_end       /* linker symbol: end of BSS   */

    .section .text.entry
    .global _module_entry
    .type   _module_entry, @function
_module_entry:
    /* save kst — %rdi is clobbered by rep stosb below */
    pushq   %rdi

    /* zero BSS so newlib's internal globals (and _kst itself) start at 0 */
    leaq    __bss_start(%rip), %rdi
    leaq    __bss_end(%rip),   %rax
    subq    %rdi, %rax          /* rax = BSS size */
    jz      .Lbss_done
    movq    %rax, %rcx
    xorl    %eax, %eax
    rep     stosb               /* memset(bss_start, 0, bss_size) */
.Lbss_done:

    /* restore kst and store it in the now-zeroed _kst global */
    popq    %rdi
    leaq    _kst(%rip), %rax
    movq    %rdi, (%rax)

    /* fix _impure_ptr: point it to the actual runtime address of _impure_data */
    leaq    _impure_data(%rip), %rax
    leaq    _impure_ptr(%rip),  %rcx
    movq    %rax, (%rcx)

    /* align stack to 16 bytes before any call */
    andq    $-16, %rsp

    /* call main(0, NULL) */
    xorl    %edi, %edi          /* argc = 0   */
    xorl    %esi, %esi          /* argv = NULL */
    call    main

    /* tail: exit with main's return value */
    movl    %eax, %edi
    call    _exit

    /* unreachable safety halt */
.Lhalt:
    hlt
    jmp     .Lhalt
