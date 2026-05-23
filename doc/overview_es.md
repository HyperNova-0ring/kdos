# KDOS — Visión General del Proyecto

KDOS es un sistema operativo hobby bare-metal x86_64 escrito en C y ensamblador GAS. Arranca mediante GRUB/Multiboot2, hace la transición de modo protegido de 32 bits a modo largo de 64 bits, y ejecuta una arquitectura basada en módulos donde todo el código corre en ring 0.

## Estructura del Repositorio

```
kdos/
├── Makefile              — Orquestador de build de alto nivel
├── kernel/               — Kernel central
├── newlib/               — Runtime C y glue de libc para módulos
├── command_kern/         — Módulo shell principal (análogo a COMMAND.COM)
├── build/                — Artefactos generados
└── doc/                  — Esta documentación
```

## Filosofía de Diseño

- **Sin syscalls.** La comunicación entre el kernel y los módulos usa la Kernel Services Table (KST), una estructura de punteros a función pasada por puntero al entry point de cada módulo.
- **Solo ring 0.** Todo el código corre en nivel de privilegio 0. Todavía no hay espacio de usuario.
- **Módulos binarios planos.** Los módulos son binarios crudos cargados por GRUB como entradas `module2`. El kernel los descubre desde la estructura de información Multiboot2.
- **Memoria con identity mapping.** Dirección física == dirección virtual en todo el rango cargado (0–128 MB). Todavía no hay gestor de memoria virtual.

## Subproyectos

| Subproyecto    | Rol                                                              |
|----------------|------------------------------------------------------------------|
| `kernel/`      | Boot, HAL, PMM, heap, IDT, cargador de módulos, KST             |
| `newlib/`      | Startup CRT (`crt.S`), stubs de syscall libc (`newlib.c`)       |
| `command_kern/`| Módulo shell de entrada, lanzado después de todos los demás módulos |

## Sistema de Build

El Makefile raíz compila los tres subproyectos y ensambla la ISO final.

### Toolchain

| Herramienta         | Rol                      |
|---------------------|--------------------------|
| `x86_64-elf-gcc`    | Compilador C (freestanding) |
| `x86_64-elf-ld`     | Enlazador                |
| `x86_64-elf-objcopy`| ELF → binario crudo      |
| `x86_64-elf-ar`     | Archivador de biblioteca estática |
| `grub-mkrescue`     | Creación de imagen ISO   |
| `qemu-system-x86_64`| Emulador para pruebas    |

### CFLAGS Comunes

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -O2
```

SSE/MMX deshabilitados porque el kernel no guarda/restaura el estado FPU en cambios de contexto.  
`-mno-red-zone` requerido en el kernel porque los manejadores de interrupciones pueden dispararse en cualquier momento.

### Targets del Makefile Raíz

| Target            | Descripción                                                   |
|-------------------|---------------------------------------------------------------|
| `make`            | Compila el ELF del kernel + todos los módulos                |
| `make iso`        | Crea ISO booteable (`build/x86_64/kernel-x86_64.iso`)        |
| `make run`        | Compila la ISO y la lanza en QEMU (128 MB RAM, serial stdio) |
| `make clean`      | Elimina todos los artefactos de build                        |
| `make VERBOSE=1`  | Muestra los comandos completos de compilación/enlazado       |
| `make DEBUG=1`    | Incluye módulos de debug y agrega `-D__DEBUG__`              |

### Layout de Salida del Build

```
build/x86_64/
├── kernel.elf            — ELF del kernel
├── kernel-x86_64.iso     — ISO GRUB booteable
├── crt.o                 — Objeto CRT de arranque (de newlib/)
├── libnewlib.a           — Biblioteca estática de glue libc (de newlib/)
├── module.ld             — Linker script para módulos (de newlib/)
├── modules/
│   ├── hello.bin         — Módulo de ejemplo
│   └── command.kern.bin  — Módulo shell principal
└── iso/
    ├── boot/
    │   ├── kernel.elf
    │   └── grub/grub.cfg
    └── modules/
        ├── hello.bin
        └── command.kern.bin
```

## Formato Binario de Módulos

Cada binario de módulo tiene este layout en el offset 0:

```
[module_header_t]     — 56 bytes: magic, name[32], version[16], reserved
[función de entrada]  — comienza inmediatamente después del header
[resto del código]    — .text, .rodata, .data, .bss, .got
```

El kernel calcula `entry = module_start + sizeof(module_header_t)` y llama:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

## Kernel Services Table (KST)

La KST (`kst_t`, definida en `kernel/module_abi.h`) es el contrato entre el kernel y los módulos. Reemplaza a las syscalls.

```c
typedef struct {
    uint32_t version;       // KST_VERSION = 1
    struct { ... } console; // print, putchar, print_hex, print_dec, clear, getchar
    struct { ... } mem;     // sbrk
    struct { ... } io;      // write, read, open, close, isatty, lseek, fstat
    struct { ... } sys;     // exit, panic, getpid
} kst_t;
```

Implementadas (no stub):
- `console.print/putchar/print_hex/print_dec/clear` → modo texto VGA
- `io.write` (fd 1, 2) → consola VGA
- `io.isatty` (fd 0–2) → devuelve 1
- `mem.sbrk` → heap bump-allocator del kernel
- `sys.panic` → kernel panic con pantalla roja + halt
- `sys.exit` → provoca kernel panic (todavía sin modelo de procesos)

Stubs (devuelven -1 o valor seguro):
- `console.getchar`, `io.read`, `io.open`, `io.close`, `io.lseek`, `io.fstat`, `sys.getpid`

## Secuencia de Arranque

```
GRUB (Multiboot2)
  └─ _start (32 bits, boot.S)
       ├─ valida el magic de Multiboot2
       ├─ zeroes del BSS
       ├─ configura tablas de páginas PML4/PDPT/PD (identity map 0–128 MB)
       ├─ habilita PAE, Long Mode (EFER), Paginación
       ├─ carga GDT de 64 bits
       ├─ salto lejano → long_mode_entry (64 bits)
       └─ llama a kernel_main(boot_magic, boot_addr)

kernel_main (kernel.c)
  ├─ hal_console_init()     — init VGA
  ├─ hal_console_clear()    — limpia pantalla
  ├─ hal_idt_init()         — instala 32 manejadores de excepciones
  ├─ imprime banner
  ├─ hal_arch_init()        — parsea Multiboot2: mapa de memoria + registro de módulos
  ├─ hal_mem_init()         — init PMM desde el mapa de memoria
  ├─ heap_init(kernel_end)  — heap bump-allocator de 4 MB
  ├─ imprime mapa de memoria
  ├─ modules_run_all()      — ejecuta módulos no-entry
  └─ modules_launch_entry() — lanza COMMAND.KERN o command.com → halt al retornar
```
