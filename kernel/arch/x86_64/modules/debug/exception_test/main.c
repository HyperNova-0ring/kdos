#include "module_abi.h"

__attribute__((section(".module_header")))
const module_header_t module_info = {
    .magic    = MODULE_MAGIC,
    .name     = "exception_test",
    .version  = "0.1",
    .reserved = 0,
};

/* ---- helpers ----------------------------------------------- */

/* #DE — division by zero */
static void trigger_divide_error(const hal_api_t *hal) {
    hal->print("  -> #DE (division by zero)...\n");
    volatile int zero = 0;
    volatile int r    = 42 / zero;
    (void)r;
}

/* #UD — invalid opcode via ud2 */
static void trigger_invalid_opcode(const hal_api_t *hal) {
    hal->print("  -> #UD (invalid opcode / ud2)...\n");
    __asm__ volatile ("ud2");
}

/* #PF — null pointer dereference */
static void trigger_page_fault(const hal_api_t *hal) {
    hal->print("  -> #PF (null pointer dereference)...\n");
    volatile uint8_t *null = (volatile uint8_t *)0x0;
    volatile uint8_t  val  = *null;
    (void)val;
}

/* #GP — read from a privileged MSR (rdmsr with invalid ECX = #GP) */
static void trigger_gpf(const hal_api_t *hal) {
    hal->print("  -> #GP (rdmsr with bad index)...\n");
    __asm__ volatile (
        "movl $0xDEAD, %%ecx\n\t"
        "rdmsr"
        : : : "eax", "ecx", "edx"
    );
}

/* ---- entry -------------------------------------------------- */
__attribute__((section(".text.entry")))
void module_main(const hal_api_t *hal) {
    hal->print("\n[exception_test] IDT exception test module\n");
    hal->print("----------------------------------------------\n");
    hal->print("Pick an exception to fire:\n");
    hal->print("  0 = #DE  division by zero\n");
    hal->print("  1 = #UD  invalid opcode (ud2)\n");
    hal->print("  2 = #PF  page fault (null deref)\n");
    hal->print("  3 = #GP  general protection fault\n\n");

    /* Change this constant to select which exception fires. */
#define TEST_EXCEPTION 0

#if   TEST_EXCEPTION == 0
    trigger_divide_error(hal);
#elif TEST_EXCEPTION == 1
    trigger_invalid_opcode(hal);
#elif TEST_EXCEPTION == 2
    trigger_page_fault(hal);
#elif TEST_EXCEPTION == 3
    trigger_gpf(hal);
#endif

    /* Execution must never reach here — the IDT handler halts. */
    hal->print("[exception_test] ERROR: exception was not caught!\n");
    hal->panic("IDT not installed correctly");
}
