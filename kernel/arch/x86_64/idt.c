#include "idt.h"
#include "vga.h"
#include "../../hal.h"

/* ── IDT structures ────────────────────────────────────────── */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

#define IDT_INTERRUPT_GATE 0x8E   /* P=1  DPL=0  type=1110 */
#define IDT_TRAP_GATE      0x8F   /* P=1  DPL=0  type=1111 */

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* ── External ISR entry points (defined in isr.S) ─────────── */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

static void (*const isr_table[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static const char *const exception_names[32] = {
    "#DE Division Error",
    "#DB Debug",
    "NMI",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved (15)",
    "#MF x87 Floating-Point",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XF SIMD Floating-Point",
    "Reserved (20)", "Reserved (21)", "Reserved (22)", "Reserved (23)",
    "Reserved (24)", "Reserved (25)", "Reserved (26)", "Reserved (27)",
    "Reserved (28)", "Reserved (29)",
    "#SX Security Exception",
    "Reserved (31)",
};

/* ── Gate setup ────────────────────────────────────────────── */
static void idt_set_gate(uint8_t vec, uint64_t handler, uint8_t type_attr) {
    idt[vec].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vec].selector    = 0x08;          /* kernel code segment */
    idt[vec].ist         = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vec].reserved    = 0;
}

/* ── Exception handler (called from isr_common_stub) ───────── */
void exception_handler(interrupt_frame_t *f) {
    const char *name = (f->vector < 32) ? exception_names[f->vector]
                                        : "Unknown";

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_print("\n\n  EXCEPTION: ");
    vga_print(name);
    vga_print("\n");

    vga_print("  RIP: "); vga_print_hex(f->rip);
    vga_print("  CS:  "); vga_print_hex(f->cs);   vga_print("\n");
    vga_print("  RSP: "); vga_print_hex(f->rsp);
    vga_print("  SS:  "); vga_print_hex(f->ss);   vga_print("\n");
    vga_print("  RFLAGS: "); vga_print_hex(f->rflags); vga_print("\n");

    if (f->error_code) {
        vga_print("  ERR: "); vga_print_hex(f->error_code); vga_print("\n");
    }

    vga_print("  RAX: "); vga_print_hex(f->rax);
    vga_print("  RBX: "); vga_print_hex(f->rbx); vga_print("\n");
    vga_print("  RCX: "); vga_print_hex(f->rcx);
    vga_print("  RDX: "); vga_print_hex(f->rdx); vga_print("\n");

    hal_cpu_halt();
}

/* ── Public init ───────────────────────────────────────────── */
void idt_init(void) {
    for (int i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, (uint64_t)isr_table[i], IDT_INTERRUPT_GATE);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}
