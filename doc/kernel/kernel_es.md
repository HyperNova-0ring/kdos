# Documentación del Kernel KDOS

El kernel es el núcleo de KDOS. Gestiona el arranque, la abstracción de hardware, la gestión de memoria física, el manejo de interrupciones, el heap y el sistema de carga/despacho de módulos.

## Estructura de Directorios

```
kernel/
├── kernel.c                    — Punto de entrada: kernel_main()
├── hal.h                       — Interfaz pública de la capa de abstracción de hardware
├── module_abi.h                — Contrato KST y binario de módulos (compartido con módulos)
├── modules.h / modules.c       — Registro y despachador de módulos
├── heap.h / heap.c             — Heap del kernel (bump allocator)
├── Makefile
├── modules/
│   └── hello/
│       ├── main.c              — Módulo de ejemplo mínimo
│       └── module.ld           — Linker script del módulo
└── arch/
    └── x86_64/
        ├── boot.S              — Header Multiboot2, transición 32→64 bits, GDT, paginación
        ├── hal.c               — Implementación del HAL para x86_64
        ├── vga.h / vga.c       — Driver VGA en modo texto BIOS (80×25)
        ├── idt.h / idt.c       — Configuración de la Interrupt Descriptor Table (IDT)
        ├── isr.S               — 32 stubs ISR de excepciones de CPU
        ├── pmm.h / pmm.c       — Gestor de Memoria Física (bitmap allocator)
        ├── multiboot2.h        — Estructuras de tags Multiboot2
        ├── link.ld             — Linker script del kernel
        └── modules/debug/
            └── exception_test/ — Módulo de debug: dispara excepciones (solo DEBUG=1)
```

## Build

```bash
# Desde la raíz del repositorio:
make              # compila el ELF del kernel + todos los módulos
make DEBUG=1      # incluye el módulo exception_test + -D__DEBUG__

# Desde kernel/ directamente:
make -C kernel ARCH=x86_64
```

Salida: `kernel/build/kernel-x86_64.elf` + `kernel/build/modules/*.bin`

### CFLAGS

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -I<arch_dir> -I. -O2
```

Flags extra para módulos: `-falign-functions=1 -falign-loops=1 -falign-jumps=1 -fPIC`

---

## Secuencia de Arranque — `arch/x86_64/boot.S`

GRUB carga el ELF del kernel en modo protegido de 32 bits y salta a `_start`.

### Configuración en 32 bits

1. **Stack**: 16 KB de stack estático en BSS.
2. **Validación Multiboot2**: comprueba `eax == 0x36D76289`; se detiene si no coincide.
3. **Zero BSS**: `rep stosl` desde `bss_start` hasta `bss_end`.
4. **Tablas de páginas** (`setup_page_tables`):
   - PML4[0] → PDPT (presente + escritura)
   - PDPT[0] → PD (presente + escritura)
   - PD[0..63] → 64 × huge pages de 2 MB con bit PS (identity map 0–128 MB)
5. **PAE**: activa el bit 5 de CR4.
6. **CR3**: carga la dirección física de PML4.
7. **Long Mode**: activa el bit 8 en el MSR EFER (0xC0000080).
8. **Paginación**: activa el bit 31 en CR0.
9. **GDT**: carga GDT de 64 bits (null, código 0x08, datos 0x10).
10. **Salto lejano**: `ljmp $0x08, $long_mode_entry` — cambia al segmento de código de 64 bits.

### Entrada en 64 bits (`long_mode_entry`)

Establece todos los registros de segmento de datos a 0x10, carga los argumentos de arranque en `rdi`/`rsi`, y llama a `kernel_main(boot_magic, boot_addr)`. Al retornar: bucle `cli` + `hlt`.

---

## Capa de Abstracción de Hardware — `hal.h` / `arch/x86_64/hal.c`

`hal.h` define la interfaz neutral a la arquitectura. `hal.c` la implementa para x86_64. El núcleo del kernel (`kernel.c`, `modules.c`, `heap.c`) solo incluye `hal.h` — nunca referencia archivos específicos de arquitectura directamente.

### API de Consola

| Función                              | Descripción                      |
|--------------------------------------|----------------------------------|
| `hal_console_init()`                 | Inicializa el modo texto VGA     |
| `hal_console_putchar(char c)`        | Escribe un carácter              |
| `hal_console_print(const char*)`     | Escribe una cadena               |
| `hal_console_print_hex(uintptr_t)`   | Imprime valor de 64 bits como 0xXXX... |
| `hal_console_print_dec(size_t)`      | Imprime número decimal           |
| `hal_console_clear()`                | Limpia la pantalla               |

### API de Memoria

```c
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;   // 1 = usable, 2+ = reservado
} hal_mem_region_t;

void     hal_mem_set_map(hal_mem_region_t* map, uint32_t count);
uint32_t hal_mem_get_map(hal_mem_region_t* out, uint32_t max);
void     hal_mem_init(void);
uintptr_t hal_mem_alloc_pages(uint32_t n);   // devuelve dirección física, 0 = OOM
void      hal_mem_free_pages(uintptr_t addr, uint32_t n);
```

### API de CPU

```c
void hal_cpu_halt(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_enable_interrupts(void);
```

### Otras funciones

```c
void hal_arch_init(uint64_t boot_magic, uint64_t boot_addr);  // parsea Multiboot2
void hal_idt_init(void);
void hal_panic(const char* msg);  // deshabilita interrupciones, pantalla roja, halt
```

---

## Driver VGA — `arch/x86_64/vga.c`

Modo texto VGA BIOS: 80 columnas × 25 filas en la dirección física `0xB8000`. Cada celda es un valor de 16 bits: byte bajo = carácter ASCII, byte alto = color (4 bits bg | 4 bits fg).

### Colores

16 colores definidos en el enum `vga_color`: `VGA_BLACK` (0) hasta `VGA_WHITE` (15).

### API

| Función                              | Descripción                                          |
|--------------------------------------|------------------------------------------------------|
| `vga_init()`                         | Inicializa, establece blanco sobre negro, limpia     |
| `vga_clear()`                        | Rellena todas las celdas con espacios, resetea cursor|
| `vga_putchar(char c)`                | Escribe carácter; maneja `\n`, `\r`, `\t`, wrap, scroll |
| `vga_print(const char* str)`         | Escribe cadena                                       |
| `vga_print_hex(uint64_t)`            | Imprime `0xXXXXXXXXXXXXXXXX`                        |
| `vga_set_color(vga_color fg, bg)`    | Establece color de primer plano + fondo              |

Scroll: cuando `cursor_row >= 25`, copia la fila N a la fila N-1 para todas las filas, limpia la fila 24.

---

## Gestor de Memoria Física — `arch/x86_64/pmm.c`

Asignador de frames de páginas físicas basado en bitmap.

### Constantes

- `PAGE_SIZE = 4096`
- `MAX_FRAMES = 512 MB / 4 KB = 131072` (bitmap de 16 KB en BSS)

### Inicialización (`pmm_init`)

1. Inicio: marca todos los frames como usados (`0xFF`).
2. Determina `total_frames` desde la dirección más alta en una región de memoria usable de Multiboot2.
3. Marca como libre cada dirección alineada a páginas dentro de regiones usables.
4. Re-marca `[0, kernel_end)` como usado — protege el área BIOS, código/datos/BSS del kernel.

### Asignación (`pmm_alloc_pages`)

Búsqueda first-fit: encuentra `n` frames contiguos libres, los marca como usados, devuelve su dirección física base. Devuelve 0 en caso de OOM.

### Liberación (`pmm_free_pages`)

Marca `n` frames a partir de `addr` como libres.

---

## Heap del Kernel — `heap.c`

Un allocator de tipo bump (lineal) respaldado por el PMM, que implementa la interfaz `sbrk`.

### Constantes

- `HEAP_PAGES = 1024` → **4 MB** de heap

### Inicialización (`heap_init`)

Redondea `base` (= `&kernel_end`) hacia arriba al límite de página, luego llama a `hal_mem_alloc_pages(1024)`. Entra en pánico si la dirección física devuelta no coincide con `heap_base` (indicaría que el identity mapping está roto).

### `heap_sbrk(intptr_t incr)`

- Avanza el puntero bump `incr` bytes.
- Devuelve el puntero antiguo (semántica POSIX de sbrk).
- Devuelve `(void*)-1` si `incr <= 0` o si excedería `heap_limit`.

Está conectado a `kst->mem.sbrk`, por lo que `malloc` en módulos enlazados con newlib funciona automáticamente.

---

## IDT y Excepciones — `arch/x86_64/idt.c` + `isr.S`

### Estructura IDT

256 entradas, cada una un `idt_entry_t` de 8 bytes con un offset de handler de 64 bits dividido, selector y atributos de tipo. El puntero IDT se carga con `lidt`.

Solo se instalan los vectores 0–31 (excepciones de CPU). Todos usan `IDT_INTERRUPT_GATE` (0x8E: P=1, DPL=0, tipo=1110), lo que significa que las interrupciones se deshabilitan automáticamente al entrar.

### Stubs ISR (`isr.S`)

Dos macros mantienen los stubs uniformes:

- `ISR_NOERR num` — empuja código de error dummy `0`, luego el número de vector, salta a `isr_common_stub`.
- `ISR_ERR num` — la CPU ya empujó un código de error; solo empuja el número de vector, salta a `isr_common_stub`.

Vectores con código de error: 8 (#DF), 10 (#TS), 11 (#NP), 12 (#SS), 13 (#GP), 14 (#PF), 17 (#AC), 30 (#SX).

### Stub Común

`isr_common_stub` guarda los 15 registros de propósito general (`rax`–`r15`), pasa `rsp` (que ahora apunta a un `interrupt_frame_t` completo) como primer argumento, y llama a `exception_handler`.

### `interrupt_frame_t`

```c
typedef struct {
    uint64_t r15..r8, rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    // empujado por la CPU:
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;
```

### Manejador de Excepciones

Muestra el nombre de la excepción (de una tabla de 32 cadenas), RIP, CS, RSP, SS, RFLAGS, código de error (si no es cero), RAX, RBX, RCX, RDX — todo sobre fondo rojo. Luego hace halt.

---

## Sistema de Módulos — `modules.h` / `modules.c`

### Estructuras de Datos

```c
typedef struct {
    uintptr_t      start;       // dirección física de inicio
    uintptr_t      end;         // dirección física de fin
    char           cmdline[64]; // línea de comandos del módulo GRUB
    module_state_t state;       // ABSENT / LOADED / RUNNING / FAILED
    module_header_t* header;    // puntero al header en start, o NULL
} module_t;

typedef struct {
    module_t modules[MAX_MODULES]; // MAX_MODULES = 16
    uint32_t count;
} module_list_t;
```

### Registro (`modules_register`)

Llamado desde `hal_arch_init` por cada `MB2_TAG_MODULE` encontrado en la info Multiboot2. Valida el `MODULE_MAGIC` en `start`; si no coincide, establece el estado a `FAILED` e imprime una advertencia.

### `modules_run_all()`

Itera todos los módulos con estado `LOADED` y header válido, saltando los shells de entrada (`COMMAND.KERN`, `command.com`). Por cada uno: imprime nombre + versión, luego llama a `entry(&kernel_kst)`.

### `modules_launch_entry()`

Intenta encontrar primero `COMMAND.KERN`, luego `command.com`. Lanza el primero que encuentre. Si no se encuentra ninguno, imprime un mensaje de error y retorna (el kernel entonces entra en pánico).

### Kernel Services Table — `kernel_kst`

Un `kst_t` estático definido en `modules.c`. Se rellena en tiempo de compilación con punteros a funciones HAL e implementaciones stub.

---

## ABI de Módulos — `module_abi.h`

Compartido por el kernel y todos los módulos.

### `module_header_t`

```c
typedef struct {
    uint32_t magic;      // MODULE_MAGIC = 0x444F534D ("DOSM")
    char     name[32];
    char     version[16];
    uint32_t reserved;
} __attribute__((packed)) module_header_t;
```

Tamaño total: 56 bytes. Se coloca en el offset 0 de cada binario de módulo mediante la sección `.module_header`.

### Macro `MODULE_HEADER(name, version)`

Incrusta el header en el ámbito de archivo:

```c
MODULE_HEADER("hello", "0.1");
```

Usa `__attribute__((section(".module_header"), used))` para evitar la eliminación de código muerto.

### `module_entry_fn`

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

La función de entrada siempre reside en `module_start + sizeof(module_header_t)`.

---

## Módulo de Ejemplo — `modules/hello/`

```c
#include "../../module_abi.h"

MODULE_HEADER("hello", "0.1");

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
```

Usa `module_abi.h` directamente (sin CRT ni newlib). El `__attribute__((section(".text.entry")))` asegura que la función sea la primera en `.text` después del header — directamente llamable en `start + 56`.

### Linker Script del Módulo (`modules/hello/module.ld`)

```
. = 0;
.module_header : { KEEP(*(.module_header)) }
.text ALIGN(1) : { *(.text.entry) *(.text .text.*) }
.rodata, .data, .got, .got.plt, .bss (todos ALIGN(1))
/DISCARD/: .eh_frame, .comment
```

---

## Módulo de Debug — `arch/x86_64/modules/debug/exception_test/`

Solo se incluye con `DEBUG=1`. Prueba la IDT disparando deliberadamente cuatro tipos de excepción:

| ID | Excepción | Método                          |
|----|-----------|---------------------------------|
| 0  | #DE       | División entera por cero        |
| 1  | #UD       | Instrucción `ud2`               |
| 2  | #PF       | Dereferenciar puntero nulo      |
| 3  | #GP       | `rdmsr` con índice inválido     |

La prueba activa se selecciona en tiempo de compilación con `#define TEST_EXCEPTION N`.

---

## Linker Script del Kernel — `arch/x86_64/link.ld`

```
ENTRY(_start)

. = 1M    (dirección de carga: 1 MB físico)

.multiboot  ALIGN(8)  — Header Multiboot2 (GRUB busca en los primeros 32 KB)
.text       ALIGN(4K) — Código
.rodata     ALIGN(4K)
.data       ALIGN(4K)
.bss        ALIGN(4K) — símbolos bss_start … bss_end exportados a boot.S

kernel_end = ALIGN(4K)  — exportado; usado por heap.c y pmm.c
```

El kernel se carga en el mark de 1 MB, la dirección de carga tradicional segura por encima del área de datos BIOS.
