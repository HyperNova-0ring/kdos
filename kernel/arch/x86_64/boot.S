/* ============================================================
   CONSTANTES MULTIBOOT2
   ============================================================ */
.set MULTIBOOT2_MAGIC,  0xE85250D6
.set ARCH_I386,         0
.set HEADER_LEN,        (header_end - header_start)
.set CHECKSUM,          -(MULTIBOOT2_MAGIC + ARCH_I386 + HEADER_LEN)

/* ============================================================
   MULTIBOOT HEADER
   ============================================================ */
.section .multiboot, "a"
.align 8
header_start:
    .long MULTIBOOT2_MAGIC
    .long ARCH_I386
    .long HEADER_LEN
    .long CHECKSUM
    /* end of tag */
    .word 0
    .word 0
    .long 8
header_end:

/* ============================================================
   BSS: stack and page tables
   ============================================================ */
.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.align 4096
pml4_table:  .skip 4096
pdpt_table:  .skip 4096
pd_table:    .skip 4096

/* ============================================================
   DATA
   ============================================================ */
.section .data
.align 8
multiboot_magic:
    .quad 0
multiboot_ptr:
    .quad 0

/* ============================================================
   ENTRY POINT — 32 bits
   ============================================================ */
.section .text
.code32
.global _start
_start:
    /* Setup stack */
    movl $stack_top, %esp

    /* Save multiboot pointer (ebx) in memory */
    movl %ebx, multiboot_ptr
    /* Save multiboot magic (eax) in memory */
    movl %eax, multiboot_magic

    /* Checks multiboot2 magic */
    cmpl $0x36D76289, %eax
    jne  .no_multiboot

    /* Zeroed BSS */
    movl $bss_start,  %edi
    movl $bss_end,    %ecx
    subl %edi,        %ecx
    shrl $2,          %ecx
    xorl %eax,        %eax
    rep  stosl

    call setup_page_tables

    /* Enable PAE in CR4 */
    movl %cr4,        %eax
    orl  $(1 << 5),   %eax
    movl %eax,        %cr4

    /* CR3 → PML4 */
    movl $pml4_table, %eax
    movl %eax,        %cr3

    /* Enable Long Mode in EFER MSR */
    movl $0xC0000080, %ecx
    rdmsr
    orl  $(1 << 8),   %eax
    wrmsr

    /* Enable pagination in CR0 */
    movl %cr0,        %eax
    orl  $(1 << 31),  %eax
    movl %eax,        %cr0

    /* load GDT of 64 bits */
    lgdt gdt64_pointer

    /* Far jump to 64-bit segmen */
    ljmp $0x08, $long_mode_entry

.no_multiboot:
    cli
    hlt

/* ============================================================
   SETUP PAGE TABLES
   ============================================================ */
setup_page_tables:
    /* PML4[0] → PDPT */
    movl $pdpt_table, %eax
    orl  $0b11,       %eax
    movl %eax,        pml4_table
    movl $0,          pml4_table + 4

    /* PDPT[0] → PD */
    movl $pd_table,   %eax
    orl  $0b11,       %eax
    movl %eax,        pdpt_table
    movl $0,          pdpt_table + 4

    /* PD[0] → huge page 2MB */
    movl $0b10000011, pd_table
    movl $0,          pd_table + 4

    ret

/* ============================================================
   GDT OF 64 BITS
   ============================================================ */
.section .rodata
.align 8
gdt64:
    .quad 0                                         /* null descriptor */
gdt64_code:                                         /* offset 0x08 */
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)    /* code 64-bit */
gdt64_data:                                         /* offset 0x10 */
    .quad (1<<41) | (1<<44) | (1<<47)              /* data */
gdt64_pointer:
    .word gdt64_pointer - gdt64 - 1                 /* size - 1 */
    .quad gdt64                                     /* direction */

/* ============================================================
   LONG MODE — 64 bits
   ============================================================ */
.section .text
.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax,   %ds
    movw %ax,   %es
    movw %ax,   %fs
    movw %ax,   %gs
    movw %ax,   %ss

    /* First Arg: Multiboot2 Magic */
    movq multiboot_magic, %rdi
    /* Second Arg: Multiboot2 Header. */
    movq multiboot_ptr, %rsi


    call kernel_main

    cli
.halt:
    hlt
    jmp .halt
    