# KDOS command_kern — Documentación

`command_kern/` es el módulo principal de entrada de KDOS, análogo a `COMMAND.COM` en MS-DOS. Es la primera capa interactiva del SO: proporciona un intérprete de comandos que permite cargar y ejecutar programas ELF transferidos desde el host.

## Estructura de Directorios

```
command_kern/
├── Makefile
├── main.c        — Shell interactivo: readline, dispatch, comandos
├── module.ld     — Linker script del módulo flat binary
├── transfer.h    — Declaración de transfer_recv_elf()
├── transfer.c    — Recepción de ELF por COM2 (protocolo KELF)
├── recover.h     — Declaración de exc_save() / exc_restore
└── recover.S     — Guardado de contexto para recuperación de excepciones
```

---

## Funcionalidad Actual

COMMAND.KERN implementa un intérprete de comandos interactivo completo:

| Comando | Descripción |
|---------|-------------|
| `help`  | Muestra la lista de comandos disponibles |
| `clear` | Limpia la pantalla |
| `halt`  | Detiene el sistema (llama a `kst->sys.panic`) |
| `load`  | Recibe un ELF por COM2 (protocolo KELF) y lo ejecuta |

El bucle principal:
1. Imprime el prompt `> `.
2. Lee una línea con `readline` (eco, backspace, máx. 128 chars).
3. Despacha a la función de comando correspondiente.

---

## Build

```bash
# Desde la raíz del repositorio:
make              # compila command.kern.bin como parte de all

# Desde command_kern/ directamente:
make -C command_kern ARCH=x86_64
```

### Salida

| Artefacto                          | Descripción                 |
|------------------------------------|-----------------------------|
| `build/x86_64/command.kern.elf`    | ELF intermedio              |
| `build/x86_64/command.kern.bin`    | Binario plano final         |

### Flags del Compilador

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -O2
-falign-functions=1 -falign-loops=1 -falign-jumps=1
-I../kernel
```

`-fPIC` requerido porque el módulo se carga en una dirección física arbitraria.

---

## Protocolo de Transferencia ELF — KELF

El comando `load` espera un ELF en COM2 empaquetado con el protocolo KELF:

```
Bytes 0–3 : magic = { 'K', 'E', 'L', 'F' }
Bytes 4–7 : size  = uint32_t, número total de bytes del ELF (little-endian)
Bytes 8+  : datos crudos del ELF
```

COM2 (0x2F8) se inicializa a 115200 baud, 8N1, FIFO habilitado, la primera vez que se llama a `transfer_recv_elf`. Límite máximo: 2 MB.

### Envío desde el host (Python)

```python
python3 -c "
    import sys, struct
    d = open('prog.elf','rb').read()
    sys.stdout.buffer.write(b'KELF' + struct.pack('<I', len(d)) + d)
" | nc localhost 4444
```

(QEMU expone COM2 en `tcp:4444` con `make run`.)

---

## Carga y Ejecución de ELF

Cuando `transfer_recv_elf` tiene éxito:

1. Imprime el tamaño recibido.
2. Llama a `kst->sys.elf_load(buf, size)` — el cargador ELF neutral del kernel.
3. Si `elf_load` devuelve 0: imprime el error y retorna al prompt.
4. Si tiene éxito: imprime la dirección del entry point y llama a `elf_exec`.

`elf_exec` instala un hook de excepción, llama al programa y, al retornar:
- Si no hubo excepción: imprime `"Program finished."`.
- Si hubo excepción: imprime el nombre de la excepción, RIP y código de error.

---

## Recuperación de Excepciones — `recover.S` + `exc_hook_fn`

COMMAND.KERN atrapa excepciones de los programas ELF que ejecuta sin colapsar el shell. El mecanismo:

### `exc_save()` / `exc_restore`

```c
int exc_save(void);       // guarda registros callee-saved + RSP + dirección de retorno
extern void exc_restore;  // destino IRETQ: restaura el estado guardado y retorna 1
```

`exc_save()` devuelve 0 en la primera llamada. Si el programa causa una excepción, el IDT llama al hook, que hace que IRETQ salte a `exc_restore`, la cual restaura el frame de `elf_exec` y retorna 1.

### `exc_hook_fn`

```c
static void exc_hook_fn(uint64_t vec, uint64_t rip, uint64_t err,
                         uint64_t* out_rip, uint64_t* out_rsp) {
    exc_vec = vec; exc_rip = rip; exc_err = err;
    __asm__ volatile ("leaq exc_restore(%%rip), %0" : "=r"(*out_rip));
    *out_rsp = exc_restore_rsp;
}
```

`leaq exc_restore(%rip)` resuelve la dirección en tiempo de ejecución (PCREL), correcto para un módulo PIC sin linker dinámico.

---

## BSS Zeroing — Módulo PIC Flat Binary

GRUB no inicializa a cero la memoria de los módulos flat binary. `module_main` lo hace explícitamente al inicio mediante asm inline RIP-relative (no puede usar C con `-fPIC` ya que el GOT no está relocado):

```c
__asm__ volatile (
    "leaq __bss_start(%%rip), %%rdi\n\t"
    "leaq __bss_end(%%rip),   %%rax\n\t"
    "subq %%rdi, %%rax\n\t"
    "jz   1f\n\t"
    "movq %%rax, %%rcx\n\t"
    "xorl %%eax, %%eax\n\t"
    "rep  stosb\n\t"
    "1:"
    : : : "rax", "rcx", "rdi", "memory"
);
```

`__bss_start` y `__bss_end` son exportados por `module.ld`.

---

## Linker Script — `module.ld`

```
. = 0;
.module_header : { KEEP(*(.module_header)) }
.text ALIGN(1) : { *(.text.entry) *(.text .text.*) }
.rodata ALIGN(1)
.data   ALIGN(1)
.got    ALIGN(1)
.got.plt ALIGN(1)
.bss    ALIGN(1) : {
    __bss_start = .;
    *(.bss .bss.*)
    __bss_end = .;
}
/DISCARD/ : { .eh_frame .comment .note.* }
```

Dirección base 0; el kernel lee el header en offset 0 y llama a la entrada en offset 56 (`sizeof(module_header_t)`).

---

## Cómo el Kernel lo Lanza

`modules_launch_entry()` en `kernel/modules.c` busca el módulo llamado `COMMAND.KERN` (por cmdline o `module_header_t.name`). Cuando lo encuentra:

1. Imprime `"Launching COMMAND.KERN..."`.
2. Calcula entry = `module_start + sizeof(module_header_t)` (= `module_start + 56`).
3. Llama a `entry(&kernel_kst)`.
4. COMMAND.KERN corre en bucle infinito (`for (;;) { readline + dispatch }`). Si retorna, `kernel_main` llama a `hal_panic`.

---

## ABI Utilizada

`command_kern` usa `module_abi.h` directamente, sin CRT ni newlib. Sigue el ABI bare de módulo:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

La KST provee todas las funciones necesarias: consola, heap (para el buffer de recepción ELF), `elf_load`, y el hook de excepciones.

---

## Configuración de GRUB

El Makefile raíz genera `grub.cfg` automáticamente durante `make iso`. Por cada `.bin` en `build/x86_64/iso/modules/`:

```
module2 /modules/command.kern.bin command.kern.bin
```

GRUB pasa el nombre del archivo como cmdline. El kernel lo almacena en `module_t.cmdline` y también lo identifica por `module_header_t.name = "COMMAND.KERN"`.
