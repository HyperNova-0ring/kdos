# KDOS newlib — Documentación

`newlib/` proporciona un entorno de runtime C (CRT) y un glue de syscalls libc que permite a los módulos usar funciones de la biblioteca C estándar como `printf`, `malloc` y `exit`. Hace de puente entre la libc de newlib y la Kernel Services Table (KST) de KDOS.

## Propósito

Sin esta capa, un módulo debe llamar directamente a las funciones KST (`kst->console.print(...)`, etc.). Con newlib enlazado, el módulo puede escribir C estándar:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("¡Hola desde un módulo C estándar!\n");
    return 0;
}
```

## Estructura de Directorios

```
newlib/
├── Makefile
├── newlib.h              — Macro MODULE_DEFINE
├── newlib.c              — Stubs de syscall (write, read, sbrk, exit, ...)
└── arch/
    └── x86_64/
        ├── crt.S         — Startup CRT: _module_entry() → main()
        └── module.ld     — Linker script para módulos con CRT
```

## Build

```bash
make -C newlib ARCH=x86_64
```

### Salida

| Artefacto                    | Descripción                                      |
|------------------------------|--------------------------------------------------|
| `build/x86_64/crt.o`         | Objeto de startup CRT                            |
| `build/x86_64/libnewlib.a`   | Biblioteca estática de glue de syscalls          |
| `build/x86_64/module.ld`     | Linker script de módulos (copia al build raíz)   |

Estos son copiados a `build/x86_64/` por el Makefile raíz.

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -fvisibility=hidden -O2
-I../kernel -I. -isystem <newlib>/include
```

`-fPIC` es requerido porque los módulos se cargan en direcciones físicas arbitrarias.  
`-fvisibility=hidden` evita la exportación accidental de símbolos de la capa de glue.

### Requisito de Toolchain

El sistema host debe tener instalado un compilador cruzado de newlib en `/usr/local/x86_64-elf/`. El Makefile lo referencia mediante `-isystem $(NEWLIB)/include` para incluir cabeceras y, implícitamente, para enlazar `libc.a` al construir un módulo completo.

---

## Startup CRT — `arch/x86_64/crt.S`

El kernel llama a cada módulo como:

```c
void _module_entry(const kst_t* kst);   // kst en %rdi (ABI SysV)
```

`crt.S` implementa `_module_entry` y realiza los siguientes pasos antes de llamar a `main`:

### Paso a paso

1. **Guardar `kst`** — `%rdi` será sobreescrito por el bucle de zeroing del BSS, por lo que se guarda en el stack.

2. **Zero BSS** — Mediante `rep stosb` desde `__bss_start` hasta `__bss_end`. Esto es necesario porque los módulos son binarios crudos cargados en direcciones arbitrarias; su BSS no está pre-inicializado a cero por el SO.

3. **Almacenar `kst` en `_kst`** — El global `_kst` de `newlib.c` (ahora en cero) se establece al puntero `kst` guardado, usando direccionamiento relativo a RIP.

4. **Corregir `_impure_ptr`** — El modelo reentrante de newlib requiere que `_impure_ptr` apunte al struct `_impure_data` real. Como el módulo es PIC y tiene layout relativo a la dirección 0, este puntero se corrige en tiempo de ejecución.

5. **Alineación del stack a 16 bytes** — `andq $-16, %rsp` satisface el requisito del ABI SysV AMD64 antes de cualquier instrucción `call`.

6. **Llamar `main(0, NULL)`** — `argc = 0`, `argv = NULL`.

7. **Llamar `_exit(valor_retorno_main)`** — nunca retorna; se enruta a través de `kst->sys.exit`.

### Halt de Seguridad

Después de `_exit` hay un bucle `hlt` inalcanzable como medida defensiva.

---

## Glue de Syscalls — `newlib.c`

Este archivo proporciona las implementaciones de syscalls específicas del SO que la libc de newlib espera. Newlib 4.x usa el modelo reentrante internamente (`_write_r`, `_read_r`, etc.) pero aún espera que la capa del SO defina los nombres sin prefijo.

### Estado Global

```c
const kst_t* _kst;   // establecido por crt.S antes de llamar a main()
```

Todas las funciones comprueban si `_kst == NULL` y devuelven `ENOSYS` si se llaman antes de que la KST esté establecida.

### Funciones I/O

| Función               | Respaldada por      | Notas                               |
|-----------------------|---------------------|-------------------------------------|
| `write(fd, buf, n)`   | `kst->io.write`     | fd 1/2 → VGA; otros devuelven EIO  |
| `read(fd, buf, n)`    | `kst->io.read`      | Stub: devuelve EIO                 |
| `open(path, ...)`     | `kst->io.open`      | Stub: devuelve ENOENT              |
| `close(fd)`           | `kst->io.close`     | Stub: devuelve EBADF               |
| `isatty(fd)`          | `kst->io.isatty`    | fd 0–2 devuelve 1                  |
| `lseek(fd, ...)`      | `kst->io.lseek`     | Stub: devuelve ESPIPE              |
| `fstat(fd, st)`       | `kst->io.fstat`     | Stub                               |

### Memoria

```c
void* sbrk(ptrdiff_t incr);   // → kst->mem.sbrk → heap_sbrk en el kernel
```

Esto permite `malloc`/`free` en el módulo ya que el allocator de newlib usa `sbrk` internamente.

### Proceso

```c
int  getpid(void);           // → kst->sys.getpid (stub: 1)
int  kill(int pid, int sig); // SIGABRT (6) → kst->sys.panic; otros → EINVAL
void _exit(int status);      // → kst->sys.exit → kernel panic
```

---

## Macro de Header de Módulo — `newlib.h`

```c
#define MODULE_DEFINE(name_str, version_str)
```

Incrusta un `module_header_t` en el ámbito de archivo usando `__attribute__((section(".module_header"), used))`. Usa esta macro en lugar de `MODULE_HEADER` de `module_abi.h` cuando compilas con el CRT.

Ejemplo:

```c
MODULE_DEFINE("miapp", "1.0");

int main(void) {
    printf("¡Hola!\n");
    return 0;
}
```

---

## Linker Script de Módulos — `arch/x86_64/module.ld`

Usado al enlazar módulos con el CRT. Layout en el offset 0:

```
. = 0;
.module_header          — Salida de la macro MODULE_DEFINE / MODULE_HEADER (56 bytes)
.text ALIGN(1):
    *(.text.entry)      — crt.o: _module_entry DEBE ser el PRIMERO en .text
    *(.text .text.*)    — código del módulo
.rodata ALIGN(1)
.data   ALIGN(1)
.got    ALIGN(1)        — requerido para PIC
.got.plt ALIGN(1)
.bss    ALIGN(1):       — símbolos __bss_start … __bss_end (usados por crt.S)
/DISCARD/: .eh_frame, .comment, .note.*
```

La restricción crítica: `_module_entry` de `crt.o` debe estar en el offset `sizeof(module_header_t)` (56). El enlazador lo logra colocando `.text.entry` primero en `.text`, inmediatamente después de `.module_header`.

---

## Cómo Compilar un Módulo con CRT + Newlib

```bash
x86_64-elf-gcc \
    -ffreestanding -mno-red-zone -fno-stack-protector \
    -mno-sse -mno-sse2 -mno-mmx -fPIC -O2 \
    -I../kernel -I../newlib \
    -isystem /usr/local/x86_64-elf/include \
    -c main.c -o main.o

x86_64-elf-ld -T build/x86_64/module.ld -nostdlib \
    build/x86_64/crt.o main.o build/x86_64/libnewlib.a \
    /usr/local/x86_64-elf/lib/libc.a \
    -o mimodulo.elf

x86_64-elf-objcopy -O binary mimodulo.elf mimodulo.bin
```

El orden de enlazado importa: `crt.o` debe ir antes de `main.o` para que `_module_entry` quede primero en `.text.entry`.

---

## Relación con el Kernel

```
Kernel
  └─ modules_run_all() / modules_launch_entry()
       └─ llama entry = module_start + 56  (= _module_entry en crt.o)
            └─ crt.S: zeros BSS, establece _kst, corrige _impure_ptr, llama main()
                 └─ main() del módulo usa printf/malloc/exit
                      └─ libc newlib → write/sbrk/_exit en newlib.c
                           └─ newlib.c → kst->io.write / kst->mem.sbrk / kst->sys.exit
                                └─ KST → HAL (VGA, heap, panic)
```
