#include "idt.h"
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

/* ── TSS (64-bit, 104 bytes) ───────────────────────────────── */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;          /* IST entry 1 — used by #DF */
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss64_t;

#define IST1_SIZE 4096

static uint8_t  ist1_stack[IST1_SIZE] __attribute__((aligned(16)));
static tss64_t  kernel_tss;

/* GDT: null | code(0x08) | data(0x10) | tss_lo(0x18) | tss_hi(0x20)
   Mirrors the values from boot.S and adds room for the TSS descriptor. */
static uint64_t kernel_gdt[5] = {
    0,
    (1ULL<<43)|(1ULL<<44)|(1ULL<<47)|(1ULL<<53),   /* 0x08: 64-bit code */
    (1ULL<<41)|(1ULL<<44)|(1ULL<<47),               /* 0x10: data        */
    0,   /* 0x18: TSS low  — filled in idt_init */
    0,   /* 0x20: TSS high — filled in idt_init */
};

static idt_ptr_t gdt_ptr;
static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;
static exc_hook_t  exc_hook = NULL;

void idt_set_exc_hook(exc_hook_t fn)  { exc_hook = fn;   }
void idt_clear_exc_hook(void)         { exc_hook = NULL; }

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
    /* Program-level hook: let command_kern recover instead of panicking */
    if (exc_hook) {
        exc_hook_t fn = exc_hook;
        exc_hook = NULL;    /* auto-clear: prevent re-entry if hook faults */
        uint64_t new_rip = 0, new_rsp = 0;
        fn(f->vector, f->rip, f->error_code, &new_rip, &new_rsp);
        if (new_rip) {
            f->rip    = new_rip;
            f->rsp    = new_rsp;
            f->rflags = 0x202;  /* IF=1, reserved bit 1 set */
        }
        return;
    }

    /* Kernel exception — use HAL so the message appears on both VGA and serial */
    const char *name = (f->vector < 32) ? exception_names[f->vector] : "Unknown";

    hal_console_panic_color();
    hal_console_print("\n\n  EXCEPTION: ");
    hal_console_print(name);
    hal_console_print("\n");

    hal_console_print("  RIP: "); hal_console_print_hex(f->rip);
    hal_console_print("  CS:  "); hal_console_print_hex(f->cs);   hal_console_print("\n");
    hal_console_print("  RSP: "); hal_console_print_hex(f->rsp);
    hal_console_print("  SS:  "); hal_console_print_hex(f->ss);   hal_console_print("\n");
    hal_console_print("  RFLAGS: "); hal_console_print_hex(f->rflags); hal_console_print("\n");

    if (f->error_code) {
        hal_console_print("  ERR: "); hal_console_print_hex(f->error_code); hal_console_print("\n");
    }

    hal_console_print("  RAX: "); hal_console_print_hex(f->rax);
    hal_console_print("  RBX: "); hal_console_print_hex(f->rbx); hal_console_print("\n");
    hal_console_print("  RCX: "); hal_console_print_hex(f->rcx);
    hal_console_print("  RDX: "); hal_console_print_hex(f->rdx); hal_console_print("\n");

    hal_cpu_halt();
}

/* ── TSS descriptor helper ─────────────────────────────────── */
static void gdt_set_tss_entry(uint64_t base, uint32_t limit) {
    /* Low 8 bytes of the 16-byte system segment descriptor */
    kernel_gdt[3] =
          (uint64_t)(limit & 0xFFFF)               /* [15:0]  limit low  */
        | ((base & 0xFFFFFFULL) << 16)             /* [39:16] base[23:0] */
        | (0x89ULL << 40)                          /* [47:40] P=1 DPL=0 type=9 */
        | (((uint64_t)((limit >> 16) & 0xF)) << 48) /* [51:48] limit high */
        | (((base >> 24) & 0xFFULL) << 56);        /* [63:56] base[31:24] */
    /* High 8 bytes */
    kernel_gdt[4] = (base >> 32) & 0xFFFFFFFFULL;
}

/* ── Public init ───────────────────────────────────────────── */
void idt_init(void) {
    /* ── Install all 32 CPU exception handlers ── */
    for (int i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, (uint64_t)isr_table[i], IDT_INTERRUPT_GATE);

    /* ── TSS: give #DF its own stack (IST1) so a kernel stack overflow
       causes a handled double fault instead of a triple fault ── */
    kernel_tss.ist1      = (uint64_t)(ist1_stack + IST1_SIZE);  /* top of IST1 */
    kernel_tss.iopb_offset = (uint16_t)sizeof(kernel_tss);

    gdt_set_tss_entry((uint64_t)&kernel_tss, (uint32_t)sizeof(kernel_tss) - 1);

    gdt_ptr.limit = (uint16_t)(sizeof(kernel_gdt) - 1);
    gdt_ptr.base  = (uint64_t)kernel_gdt;
    __asm__ volatile ("lgdt %0" : : "m"(gdt_ptr));
    __asm__ volatile ("ltr %0"  : : "r"((uint16_t)0x18));

    /* Vector 8 (#DF) uses IST1 */
    idt[8].ist = 1;

    /* ── Load IDT ── */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}
