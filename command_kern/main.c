#include "../kernel/module_abi.h"
#include "transfer.h"
#include "recover.h"

MODULE_HEADER("COMMAND.KERN", "1.0");

/* ── String helpers ──────────────────────────────────────── */

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

/* ── Line reader ─────────────────────────────────────────── */

#define LINE_MAX 128

static int readline(const kst_t* kst, char* buf) {
    int len = 0;
    for (;;) {
        int c = kst->console.getchar();

        if (c == '\r' || c == '\n') {
            kst->console.putchar('\n');
            buf[len] = '\0';
            return len;
        }
        if (c == '\b' || c == 0x7F) {
            if (len > 0) { len--; kst->console.print("\b \b"); }
            continue;
        }
        if (c >= 0x20 && len < LINE_MAX - 1) {
            buf[len++] = (char)c;
            kst->console.putchar((char)c);
        }
    }
}

/* ── Program-level exception handler ────────────────────── */

static volatile int      exc_caught;
static volatile uint64_t exc_vec;
static volatile uint64_t exc_rip;
static volatile uint64_t exc_err;
/* Captured inside elf_exec; must be static so the compiler uses RIP-relative
   addressing (not an absolute via GOT) for the -fPIC PIC module. */
static volatile uint64_t exc_restore_rsp;

static const char* exc_name(uint64_t v) {
    static const char* tbl[32] = {
        "#DE Divide Error",        "#DB Debug",
        "NMI",                     "#BP Breakpoint",
        "#OF Overflow",            "#BR Bound Range",
        "#UD Invalid Opcode",      "#NM No FPU",
        "#DF Double Fault",        "Coproc Seg Overrun",
        "#TS Invalid TSS",         "#NP Seg Not Present",
        "#SS Stack Fault",         "#GP General Protection",
        "#PF Page Fault",          "Reserved",
        "#MF x87 FP",              "#AC Alignment Check",
        "#MC Machine Check",       "#XF SIMD FP",
        "Reserved","Reserved","Reserved","Reserved",
        "Reserved","Reserved","Reserved","Reserved",
        "Reserved","Reserved",     "#SX Security",
        "Reserved",
    };
    return (v < 32) ? tbl[v] : "Unknown";
}

static void exc_hook_fn(uint64_t vec, uint64_t rip, uint64_t err,
                         uint64_t* out_rip, uint64_t* out_rsp) {
    exc_vec  = vec;
    exc_rip  = rip;
    exc_err  = err;
    /* leaq gives a PCREL-resolved runtime address — (uint64_t)exc_restore
       would produce a link-time constant and break in a PIC module. */
    __asm__ volatile ("leaq exc_restore(%%rip), %0" : "=r"(*out_rip));
    *out_rsp = exc_restore_rsp;
}

/* noinline ensures elf_exec has its own stable frame for exc_save/exc_restore.
   exc_restore fires as the IRETQ target and returns 1 to exc_save's call site,
   jumping here with the saved callee registers and RSP fully restored. */
__attribute__((noinline))
static void elf_exec(const kst_t* kst, uintptr_t entry) {
    typedef void (*prog_fn_t)(const kst_t*);

    exc_caught = 0;

    /* Capture RSP now (before exc_save call adjusts it) for the IRETQ frame */
    __asm__ volatile ("movq %%rsp, %0" : "=m"(exc_restore_rsp));

    kst->sys.set_exc_hook(exc_hook_fn);

    if (exc_save() == 0) {
        ((prog_fn_t)entry)(kst);
        kst->sys.clear_exc_hook();
    } else {
        /* exc_restore fired: arrived here with the full elf_exec frame intact */
        exc_caught = 1;
    }
}

/* ── Commands ────────────────────────────────────────────── */

static void cmd_help(const kst_t* kst) {
    kst->console.print(
        "Commands:\n"
        "  help   -- show this help\n"
        "  clear  -- clear the screen\n"
        "  halt   -- halt the system\n"
        "  load   -- receive ELF via COM2 (KELF protocol) and run it\n"
    );
}

static void cmd_load(const kst_t* kst) {
    kst->console.print("Waiting for ELF on COM2 (KELF protocol)...\n");

    uint8_t*  buf;
    uint32_t  size;
    if (!transfer_recv_elf(kst, &buf, &size)) {
        kst->console.print("Transfer error: bad magic, size out of range, or OOM.\n");
        return;
    }

    kst->console.print("Received ");
    kst->console.print_dec(size);
    kst->console.print(" bytes. Loading ELF...\n");

    uintptr_t entry = kst->sys.elf_load(buf, size);
    if (!entry) {
        kst->console.print("ELF load failed: invalid or unsupported binary.\n");
        return;
    }

    kst->console.print("Entry at 0x");
    kst->console.print_hex(entry);
    kst->console.print(". Running...\n");

    elf_exec(kst, entry);

    if (exc_caught) {
        kst->console.print("\nException: ");
        kst->console.print(exc_name(exc_vec));
        kst->console.print("\n  RIP=0x");
        kst->console.print_hex(exc_rip);
        if (exc_err) {
            kst->console.print("  ERR=0x");
            kst->console.print_hex(exc_err);
        }
        kst->console.putchar('\n');
    } else {
        kst->console.print("Program finished.\n");
    }
}

static void cmd_unknown(const kst_t* kst, const char* cmd) {
    kst->console.print("Unknown command: ");
    kst->console.print(cmd);
    kst->console.putchar('\n');
}

static void dispatch(const kst_t* kst, const char* line) {
    if (line[0] == '\0')          return;
    if (str_eq(line, "help"))  { cmd_help(kst);  return; }
    if (str_eq(line, "clear")) { kst->console.clear(); return; }
    if (str_eq(line, "halt"))  { kst->sys.panic("halt requested"); }
    if (str_eq(line, "load"))  { cmd_load(kst);  return; }
    cmd_unknown(kst, line);
}

/* ── Entry ───────────────────────────────────────────────── */

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    /* Zero BSS — GRUB does not zero flat binary module memory.
       Use RIP-relative leaq so addresses are load-position-correct
       without a dynamic linker or GOT relocation. */
    __asm__ volatile (
        "leaq __bss_start(%%rip), %%rdi\n\t"
        "leaq __bss_end(%%rip),   %%rax\n\t"
        "subq %%rdi, %%rax\n\t"
        "jz   1f\n\t"
        "movq %%rax, %%rcx\n\t"
        "xorl %%eax, %%eax\n\t"
        "rep  stosb\n\t"
        "1:"
        : : : "rax", "rcx", "rdi", "memory"
    );

    char line[LINE_MAX];

    kst->console.clear();
    kst->console.print("KDOS COMMAND.KERN 1.0\nType 'help' for available commands.\n\n");

    for (;;) {
        kst->console.print("> ");
        readline(kst, line);
        dispatch(kst, line);
    }
}
