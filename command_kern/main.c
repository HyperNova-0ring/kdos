#include "../kernel/module_abi.h"

MODULE_HEADER("COMMAND.KERN", "1.0");

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
