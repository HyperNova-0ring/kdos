#ifndef NEWLIB_H
#define NEWLIB_H

#include "../kernel/module_abi.h"

/*
 * _kst — pointer to the Kernel Services Table.
 * Set by crt.S before main() is called.
 * Available to programs that need direct kernel service access
 * beyond what the standard C library provides.
 */
extern const kst_t* _kst;

#endif
