#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Stack frame pushed by ISR stubs + CPU on exception entry.
   Layout matches the push order in isr.S (lowest address first). */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    /* pushed by the CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

void idt_init(void);

/* Program-level exception hook — if set, fires instead of kernel panic.
   Auto-cleared on entry to prevent re-entrant faults.
   The hook fills *out_rip / *out_rsp with the recovery address and stack;
   if *out_rip is non-zero the handler redirects the IRETQ frame there. */
typedef void (*exc_hook_t)(uint64_t vec, uint64_t rip, uint64_t err,
                            uint64_t* out_rip, uint64_t* out_rsp);
void idt_set_exc_hook(exc_hook_t fn);
void idt_clear_exc_hook(void);

#endif
