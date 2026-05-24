# KDOS — Visión General del Metaproyecto

KDOS es un sistema operativo bare-metal de tipo DOS (DOS-like) escrito en C y ensamblador GAS. Arranca mediante GRUB/Multiboot2, transiciona de modo protegido de 32 bits a modo largo de 64 bits, y ejecuta todos los subsistemas como módulos binarios en ring 0.

El objetivo final es un entorno interactivo análogo a MS-DOS: un intérprete de comandos (COMMAND.KERN) que lanza programas ELF estándar usando la C library (newlib) como capa de acceso al kernel.

---

## Filosofía de Diseño

| Principio | Descripción |
|-----------|-------------|
| **Sin syscalls** | El kernel se comunica con los módulos mediante la KST (Kernel Services Table), una estructura de punteros a función pasada directamente a cada entry point. |
| **Solo ring 0** | Todo el código corre en privilegio 0. No existe espacio de usuario todavía. |
| **Kernel neutral + capa arch** | `kernel/*.c/h` es código portable e independiente de arquitectura. Solo ve `hal.h`. El detalle específico de cada arquitectura vive en `kernel/arch/{arch}/`. |
| **ELF como servicio del kernel** | El cargador ELF es parte del kernel neutral y se expone a los módulos vía `kst->sys.elf_load`. Ningún módulo necesita implementar su propio loader. |
| **Newlib como capa C estándar** | Los programas se escriben en C estándar (`main`, `printf`, `malloc`). El CRT y el glue de newlib conectan automáticamente la libc con la KST. |
| **Identity mapping** | Dirección física == dirección virtual en 0–128 MB. La KST es un puntero a un `kst_t` estático en el kernel, que nunca se mueve en memoria: acceder a la KST es tan simple como desreferenciar un puntero. |

---

## Estructura del Repositorio

```
kdos/
├── Makefile              — Orquestador de build de alto nivel
├── kernel/               — Kernel central
│   ├── *.c / *.h         — Capa neutral y portable (hal.h, elf, heap, módulos)
│   └── arch/x86_64/      — Implementación específica de x86_64
├── newlib/               — CRT + glue de libc para programas
├── command_kern/         — Módulo shell principal (COMMAND.KERN)
├── test/                 — Programas de ejemplo para cargar vía `load`
├── build/                — Artefactos generados
└── doc/                  — Documentación
```

---

## Capas del Sistema

```
┌─────────────────────────────────────────────────────────┐
│              Programas ELF (main + newlib)               │
│        printf / malloc / read / write → KST             │
├─────────────────────────────────────────────────────────┤
│                  COMMAND.KERN                           │
│    readline · dispatch · load (kst->sys.elf_load)       │
├─────────────────────────────────────────────────────────┤
│       Kernel neutral  (kernel/*.c)                      │
│  modules · heap · elf_load · KST (kst_t)               │
├─────────────────────────────────────────────────────────┤
│       HAL  (kernel/hal.h ↔ arch/x86_64/hal.c)          │
│  console · PMM · CPU · IDT · panic                      │
├─────────────────────────────────────────────────────────┤
│   Hardware  (VGA · Serial · Teclado · PMM · IDT)        │
└─────────────────────────────────────────────────────────┘
```

---

## Subproyectos

| Subproyecto      | Rol                                                                 |
|------------------|---------------------------------------------------------------------|
| `kernel/`        | Boot, HAL, PMM, heap, IDT+TSS, cargador ELF, sistema de módulos, KST |
| `newlib/`        | CRT (`crt.S`), glue de syscalls libc (`newlib.c`)                   |
| `command_kern/`  | Shell interactivo, protocolo de transferencia ELF por COM2          |
| `test/`          | Programas de ejemplo: C estándar y ASM directo a KST                |

---

## Sistema de Build

### Toolchain

| Herramienta          | Rol                          |
|----------------------|------------------------------|
| `x86_64-elf-gcc`     | Compilador C (freestanding)  |
| `x86_64-elf-ld`      | Enlazador                    |
| `x86_64-elf-objcopy` | ELF → binario crudo          |
| `x86_64-elf-ar`      | Archivador de biblioteca     |
| `grub-mkrescue`      | Imagen ISO booteable         |
| `qemu-system-x86_64` | Emulador para pruebas        |

### Targets principales

| Target          | Descripción                                                  |
|-----------------|--------------------------------------------------------------|
| `make`          | Kernel ELF + todos los módulos                               |
| `make iso`      | ISO GRUB booteable en `build/x86_64/kernel-x86_64.iso`       |
| `make run`      | ISO en QEMU (128 MB RAM, serial stdio + COM2 tcp:4444)       |
| `make DEBUG=1`  | Incluye módulo `exception_test` + flag `-D__DEBUG__`          |
| `make clean`    | Elimina todos los artefactos de build                        |
| `make VERBOSE=1`| Muestra comandos completos de compilación/enlazado           |

### CFLAGS comunes

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -O2
```

SSE/MMX deshabilitados porque el kernel no salva estado FPU en cambios de contexto.  
`-mno-red-zone` requerido por los manejadores de interrupciones.

---

## Formato Binario de Módulos (flat binary)

Los módulos del kernel (hello, exception_test, command_kern) son **binarios planos** cargados por GRUB como `module2`. Su layout en offset 0:

```
[module_header_t — 56 bytes]  magic + name[32] + version[16] + reserved
[función de entrada]           inmediatamente después, en offset 56
[resto del código/datos]       .text, .rodata, .data, .bss, .got
```

El kernel llama a `entry = module_start + 56` con firma:
```c
void entry(const kst_t* kst);
```

---

## Formato de Programas (ELF64)

Los programas cargados en tiempo de ejecución por COMMAND.KERN son **ELF64 ejecutables estándar** (`ET_EXEC`). Se compilan con:
- Compilador cruzado `x86_64-elf-gcc`
- CRT: `newlib/arch/x86_64/crt.S` → entry `_prog_entry`
- Libc: `libnewlib.a` + cabeceras de newlib
- Linker script: `newlib/arch/x86_64/prog.ld` (dirección base 32 MB)

El cargador ELF del kernel (`kernel/elf.c`) lee los segmentos `PT_LOAD` y los copia a sus `p_vaddr` en el espacio de memoria con identity mapping.

---

## Kernel Services Table (KST v2)

```c
typedef struct {
    uint32_t version;    // KST_VERSION = 2
    struct { print, print_hex, print_dec, putchar, clear, getchar } console;
    struct { sbrk }                                                  mem;
    struct { write, read, open, close, isatty, lseek, fstat }       io;
    struct { exit, panic, getpid, set_exc_hook, clear_exc_hook,
             elf_load }                                              sys;
} kst_t;
```

`kst->sys.elf_load(data, size)` — carga un ELF64 y devuelve su entry point.

---

## Secuencia de Arranque

```
GRUB (Multiboot2)
  └─ _start [boot.S, 32-bit]
       ├─ valida magic Multiboot2
       ├─ zeroa BSS
       ├─ configura tablas de páginas: identity map 0–128 MB (64×2MB huge pages)
       ├─ habilita PAE + Long Mode (EFER) + Paginación
       ├─ carga GDT de 64 bits, salto lejano → long_mode_entry
       └─ llama kernel_main(boot_magic, boot_addr)

kernel_main [kernel.c]
  ├─ hal_console_init()     — VGA o Serial según cmdline
  ├─ hal_idt_init()         — IDT + TSS (IST1 para #DF)
  ├─ hal_arch_init()        — parsea MB2: mapa de memoria + módulos
  ├─ hal_mem_init()         — inicializa PMM (bitmap allocator, hasta 512 MB)
  ├─ heap_init(kernel_end)  — heap bump-allocator de 4 MB sobre el PMM
  ├─ modules_run_all()      — ejecuta módulos auxiliares (no-entry)
  └─ modules_launch_entry() — lanza COMMAND.KERN → bucle interactivo
```
