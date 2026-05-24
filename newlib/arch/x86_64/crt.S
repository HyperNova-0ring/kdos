/*
 * crt.S — x86_64 CRT startup for KDOS ELF programs.
 *
 * The ELF loader calls _prog_entry(const kst_t* kst),
 * with kst in %rdi per the SysV x86_64 ABI.
 *
 * Startup sequence:
 *   1. Save kst (%rdi is clobbered by BSS zeroing).
 *   2. Zero BSS so globals start clean.
 *   3. Store kst in _kst (used by the newlib KST hooks in newlib.c).
 *   4. Init _impure_ptr -> &_impure_data (newlib reentrant state).
 *   5. Align stack to the 16-byte SysV requirement.
 *   6. Call main(0, NULL).
 *   7. Call _exit(return_value) — never returns.
 */

    .extern _kst            /* const kst_t* — defined in newlib.c         */
    .extern _impure_ptr     /* struct _reent* — provided by libc.a        */
    .extern _impure_data    /* struct _reent  — provided by libc.a        */
    .extern main
    .extern _exit
    .extern __bss_start
    .extern __bss_end

    .section .text.entry
    .global _prog_entry
    .type   _prog_entry, @function
_prog_entry:
    pushq   %rdi                        /* save kst before BSS zero clobbers rdi */

    leaq    __bss_start(%rip), %rdi
    leaq    __bss_end(%rip),   %rax
    subq    %rdi, %rax                  /* rax = BSS size */
    jz      .Lbss_done
    movq    %rax, %rcx
    xorl    %eax, %eax
    rep     stosb
.Lbss_done:

    popq    %rdi                        /* restore kst */
    leaq    _kst(%rip), %rax
    movq    %rdi, (%rax)               /* _kst = kst */

    leaq    _impure_data(%rip), %rax
    leaq    _impure_ptr(%rip),  %rcx
    movq    %rax, (%rcx)               /* _impure_ptr = &_impure_data */

    andq    $-16, %rsp                  /* 16-byte stack align */

    xorl    %edi, %edi                  /* argc = 0   */
    xorl    %esi, %esi                  /* argv = NULL */
    call    main

    movl    %eax, %edi
    call    _exit

.Lhalt:
    hlt
    jmp     .Lhalt
