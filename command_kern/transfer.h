#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>
#include "../kernel/module_abi.h"

/*
 * Receive an ELF wrapped in the KELF protocol from COM2.
 *
 * Protocol (little-endian):
 *   Bytes 0-3 : magic  = { 'K','E','L','F' }
 *   Bytes 4-7 : size   = uint32_t, total ELF byte count
 *   Bytes 8+  : raw ELF data
 *
 * Allocates the receive buffer via kst->mem.sbrk.
 * Returns 1 on success, 0 on protocol error or OOM.
 *
 * Host-side one-liner (Python):
 *   python3 -c "
 *     import sys, struct
 *     d = open('prog.elf','rb').read()
 *     sys.stdout.buffer.write(b'KELF' + struct.pack('<I', len(d)) + d)
 *   " | nc localhost 4444
 */
int transfer_recv_elf(const kst_t* kst, uint8_t** out_buf, uint32_t* out_size);

#endif
