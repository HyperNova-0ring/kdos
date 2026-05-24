# KDOS newlib — Documentación

`newlib/` proporciona un entorno de runtime C (CRT) y un glue de syscalls libc que permite escribir programas ELF para KDOS en C estándar. Hace de puente entre la libc de newlib y la Kernel Services Table (KST) de KDOS.

## Propósito

Los programas KDOS son **ELF64 estándar** (`ET_EXEC`). No son módulos flat binary — no llevan `module_header_t`. Son compilados y enlazados como ejecutables normales y transferidos al sistema en tiempo de ejecución mediante `command_kern`. El CRT y el glue de newlib permiten escribir programas con `main`, `printf`, `malloc` y `exit`:

```c
#include <stdio.h>

int main(void) {
    printf("Hola desde KDOS!\n");
    return 0;
}
```

El kernel los carga mediante `kst->sys.elf_load` (el cargador ELF neutral) y llama al entry point `_prog_entry` del CRT.

## Estructura de Directorios

```
newlib/
├── Makefile
├── newlib.h              — Expone _kst (puntero KST para acceso directo)
├── newlib.c              — Stubs de syscall (write, read, sbrk, exit, ...)
└── arch/
    └── x86_64/
        ├── crt.S         — Startup CRT: _prog_entry() → main()
        └── prog.ld       — Linker script para programas ELF
```

## Build

```bash
make -C newlib ARCH=x86_64
```

### Salida

| Artefacto                    | Descripción                                    |
|------------------------------|------------------------------------------------|
| `build/x86_64/crt.o`         | Objeto de startup CRT                          |
| `build/x86_64/libnewlib.a`   | Biblioteca estática de glue de syscalls        |
| `build/x86_64/prog.ld`       | Linker script para programas ELF               |

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -fvisibility=hidden -O2
-I../kernel -I. -isystem <newlib>/include
```

`-fPIC` porque los programas se cargan en direcciones físicas arbitrarias.  
`-fvisibility=hidden` evita la exportación accidental de símbolos del glue.

### Requisito de Toolchain

El host debe tener instalado el compilador cruzado de newlib en `/usr/local/x86_64-elf/`. El Makefile lo referencia mediante `-isystem $(NEWLIB)/include`.

---

## Startup CRT — `arch/x86_64/crt.S`

El cargador ELF del kernel salta al entry point del programa (`_prog_entry`) con la KST en `%rdi` (ABI SysV).

### Secuencia de arranque

1. **Guardar `kst`** — `%rdi` será sobreescrito por el zeroing del BSS; se guarda en el stack.
2. **Zero BSS** — `rep stosb` desde `__bss_start` hasta `__bss_end` (RIP-relative). Obligatorio porque los segmentos ELF se copian tal cual; el BSS no viene pre-inicializado.
3. **Almacenar `kst` en `_kst`** — El global `_kst` de `newlib.c` se establece al puntero guardado.
4. **Corregir `_impure_ptr`** — El modelo reentrante de newlib requiere que `_impure_ptr` apunte a `_impure_data`. Se corrige en tiempo de ejecución con RIP-relative addressing.
5. **Alineación del stack a 16 bytes** — `andq $-16, %rsp` para el ABI SysV AMD64.
6. **Llamar `main(0, NULL)`** — `argc = 0`, `argv = NULL`.
7. **Llamar `_exit(valor_retorno)`** — enruta a través de `kst->sys.exit`; nunca retorna.

Después de `_exit` hay un bucle `hlt` inalcanzable como medida defensiva.

---

## Glue de Syscalls — `newlib.c`

Proporciona las implementaciones de syscall específicas del SO que newlib espera.

### Estado Global

```c
const kst_t* _kst;   // establecido por crt.S antes de main()
```

Accesible desde el programa mediante `#include "newlib.h"` para llamadas directas a la KST más allá de lo que ofrece la libc estándar.

### Funciones I/O

| Función               | Respaldada por    | Notas                            |
|-----------------------|-------------------|----------------------------------|
| `write(fd, buf, n)`   | `kst->io.write`   | fd 1/2 → consola; otros → EIO   |
| `read(fd, buf, n)`    | `kst->io.read`    | Stub: devuelve EIO               |
| `open(path, ...)`     | `kst->io.open`    | Stub: devuelve ENOENT            |
| `close(fd)`           | `kst->io.close`   | Stub: devuelve EBADF             |
| `isatty(fd)`          | `kst->io.isatty`  | fd 0–2 devuelve 1                |
| `lseek(fd, ...)`      | `kst->io.lseek`   | Stub: devuelve ESPIPE            |
| `fstat(fd, st)`       | `kst->io.fstat`   | Stub                             |

### Memoria

```c
void* sbrk(ptrdiff_t incr);   // → kst->mem.sbrk → heap_sbrk en el kernel
```

Permite `malloc`/`free` ya que el allocator de newlib usa `sbrk` internamente. `sbrk(0)` devuelve el break actual (consulta de estado de heap por `malloc`).

### Proceso

```c
int  getpid(void);           // → kst->sys.getpid (stub: 1)
int  kill(int pid, int sig); // SIGABRT → kst->sys.panic; otros → EINVAL
void _exit(int status);      // → kst->sys.exit
```

---

## Linker Script de Programas — `arch/x86_64/prog.ld`

Usado al enlazar programas ELF para KDOS. Sin `module_header_t` — los programas son ELF estándar.

```
ENTRY(_prog_entry)
. = 0x2000000;          — dirección base: 32 MB (por encima del kernel y sus módulos)

.text   : { *(.text.entry) *(.text .text.*) }
.rodata ALIGN(8) : { ... }
.data   ALIGN(8) : { ... }
.got    ALIGN(8) : { ... }
.got.plt ALIGN(8): { ... }
.bss    ALIGN(8) : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
}
/DISCARD/ : { .eh_frame .comment .note.* }
```

La restricción crítica: `_prog_entry` de `crt.o` debe ser el primero en `.text`. El linker lo garantiza colocando `.text.entry` antes que `.text`.

---

## Cómo Compilar un Programa

```bash
x86_64-elf-gcc \
    -ffreestanding -mno-red-zone -fno-stack-protector \
    -mno-sse -mno-sse2 -mno-mmx -fPIC -O2 \
    -I../kernel -I../newlib \
    -isystem /usr/local/x86_64-elf/include \
    -c main.c -o main.o

x86_64-elf-ld -T build/x86_64/prog.ld -nostdlib \
    build/x86_64/crt.o main.o build/x86_64/libnewlib.a \
    /usr/local/x86_64-elf/lib/libc.a \
    -o miprograma.elf
```

El programa resultante es un ELF64 estándar. Se transfiere al sistema mediante el protocolo KELF de `command_kern` (ver `doc/command_kern/`).

El orden de enlazado importa: `crt.o` debe ir antes de `main.o`.

---

## Relación con el Sistema

```
command_kern (load)
  └─ transfer_recv_elf()    — recibe ELF por COM2 (protocolo KELF)
       └─ kst->sys.elf_load(buf, size)
            └─ kernel/elf.c: carga segmentos PT_LOAD en memoria identity-mapped
                 └─ devuelve e_entry (_prog_entry en crt.o)
                      └─ crt.S: zeros BSS, establece _kst, corrige _impure_ptr, llama main()
                           └─ main() usa printf/malloc/exit
                                └─ newlib libc → write/sbrk/_exit en newlib.c
                                     └─ newlib.c → kst->io.write / kst->mem.sbrk / kst->sys.exit
                                          └─ KST → HAL (consola, heap, panic)
```
