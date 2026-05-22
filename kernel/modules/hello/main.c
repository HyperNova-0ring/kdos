#include "../../module_abi.h"

__attribute__((section(".module_header")))
const module_header_t module_info = {
    .magic    = MODULE_MAGIC,
    .name     = "hello",
    .version  = "0.1",
    .reserved = 0,
};

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
