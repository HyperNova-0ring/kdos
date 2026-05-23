#ifndef NEWLIB_H
#define NEWLIB_H

#include <stdint.h>

/*
 * MODULE_DEFINE — embed the module header into the binary.
 * Call this exactly once, at file scope, in the module's main source file.
 * The programmer only writes int main(int argc, char** argv).
 */
#define MODULE_DEFINE(name_str, version_str)                            \
    __attribute__((section(".module_header"), used))                    \
    static const struct {                                               \
        uint32_t magic;                                                 \
        char     name[32];                                              \
        char     version[16];                                           \
        uint32_t reserved;                                              \
    } __attribute__((packed)) _module_hdr = {                          \
        .magic    = 0x444F534D,  /* MODULE_MAGIC */                     \
        .name     = name_str,                                           \
        .version  = version_str,                                        \
        .reserved = 0,                                                  \
    }

#endif
