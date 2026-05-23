#include "module_abi.h"

MODULE_HEADER("exception_test", "test");

/* ---- helpers ----------------------------------------------- */

static void trigger_divide_error(const kst_t* kst) {
    kst->console.print("  -> #DE (division by zero)...\n");
    volatile int zero = 0;
    volatile int r    = 42 / zero;
    (void)r;
}

static void trigger_invalid_opcode(const kst_t* kst) {
    kst->console.print("  -> #UD (invalid opcode / ud2)...\n");
    __asm__ volatile ("ud2");
}

static void trigger_page_fault(const kst_t* kst) {
    kst->console.print("  -> #PF (null pointer dereference)...\n");
    volatile uint8_t* null = (volatile uint8_t*)0x0;
    volatile uint8_t  val  = *null;
    (void)val;
}

static void trigger_gpf(const kst_t* kst) {
    kst->console.print("  -> #GP (rdmsr with bad index)...\n");
    __asm__ volatile (
        "movl $0xDEAD, %%ecx\n\t"
        "rdmsr"
        : : : "eax", "ecx", "edx"
    );
}

/* ---- entry -------------------------------------------------- */
__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("\n[exception_test] IDT exception test module\n");
    kst->console.print("----------------------------------------------\n");
    kst->console.print("Pick an exception to fire:\n");
    kst->console.print("  0 = #DE  division by zero\n");
    kst->console.print("  1 = #UD  invalid opcode (ud2)\n");
    kst->console.print("  2 = #PF  page fault (null deref)\n");
    kst->console.print("  3 = #GP  general protection fault\n\n");

#define TEST_EXCEPTION 0

#if   TEST_EXCEPTION == 0
    trigger_divide_error(kst);
#elif TEST_EXCEPTION == 1
    trigger_invalid_opcode(kst);
#elif TEST_EXCEPTION == 2
    trigger_page_fault(kst);
#elif TEST_EXCEPTION == 3
    trigger_gpf(kst);
#endif

    kst->console.print("[exception_test] ERROR: exception was not caught!\n");
    kst->sys.panic("IDT not installed correctly");
}
