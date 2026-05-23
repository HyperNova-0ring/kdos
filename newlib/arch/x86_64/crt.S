/*
 * crt.S — x86_64 module CRT startup.
 *
 * The kernel calls: _module_entry(const kst_t* kst)
 *   kst arrives in %rdi per the SysV x86_64 ABI.
 *
 * This stub:
 *   1. Stores kst in _kst (used by the newlib hooks in newlib.c).
 *   2. Aligns the stack to the 16-byte SysV requirement.
 *   3. Calls int main(int argc, char** argv) with argc=0, argv=NULL.
 *   4. Calls _exit() with main's return value — never returns.
 */

    .extern _kst        /* const kst_t* — defined in newlib.c */
    .extern main        /* int main(int, char**) — defined by the module */
    .extern _exit       /* void _exit(int) — defined in newlib.c */

    .section .text.entry
    .global _module_entry
    .type   _module_entry, @function
_module_entry:
    /* store the kst pointer for the newlib hooks */
    leaq    _kst(%rip), %rax
    movq    %rdi, (%rax)

    /* align stack to 16 bytes before any call */
    andq    $-16, %rsp

    /* call main(0, NULL) */
    xorl    %edi, %edi          /* argc = 0  */
    xorl    %esi, %esi          /* argv = NULL */
    call    main

    /* tail: exit with main's return value */
    movl    %eax, %edi
    call    _exit

    /* unreachable safety halt */
.Lhalt:
    hlt
    jmp     .Lhalt
