#ifndef RECOVER_H
#define RECOVER_H

#include <stdint.h>

/* exc_save() — saves callee-saved registers + return address + RSP.
   Returns 0 on first call; returns 1 when exc_restore fires (via IRETQ). */
int exc_save(void);

/* IRETQ target: restores state saved by exc_save() and returns 1 to its
   caller.  Pass as *out_rip in the exception hook. */
extern void exc_restore(void);

#endif
