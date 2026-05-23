# KDOS command_kern — Documentación

`command_kern/` es el módulo principal de entrada de KDOS, análogo a `COMMAND.COM` en MS-DOS. Tras finalizar su propia inicialización y ejecutar todos los módulos auxiliares, el kernel busca este módulo por nombre y le entrega la ejecución. Es la primera capa interactiva del SO.

## Estructura de Directorios

```
command_kern/
├── Makefile
├── main.c       — Código fuente del módulo
└── module.ld    — Linker script del módulo
```

## Estado Actual

El módulo es un esqueleto mínimo: declara el header como `COMMAND.KERN` versión `1.0` y su función de entrada simplemente imprime `"Hello World!\n"` a través de la consola KST. Todavía no implementa ninguna funcionalidad de shell interactivo.

```c
#include "../kernel/module_abi.h"

MODULE_HEADER("COMMAND.KERN", "1.0");

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    kst->console.print("Hello World!\n");
}
```

---

## Build

```bash
# Desde la raíz del repositorio:
make              # compila command.kern.bin como parte de all

# Desde command_kern/ directamente:
make -C command_kern ARCH=x86_64
```

### Salida

| Artefacto                             | Descripción                       |
|---------------------------------------|-----------------------------------|
| `build/x86_64/command.kern.elf`       | ELF intermedio                    |
| `build/x86_64/command.kern.bin`       | Binario plano final               |

El Makefile raíz copia `command.kern.bin` a `build/x86_64/modules/` y luego al directorio `modules/` de la ISO.

### Flags del Compilador

```
-ffreestanding -mno-red-zone -fno-stack-protector
-mno-sse -mno-sse2 -mno-mmx -fPIC -O2
-falign-functions=1 -falign-loops=1 -falign-jumps=1
-I../kernel
```

`-fPIC` es requerido porque el módulo se carga en una dirección física arbitraria.  
Los flags de alineación estricta (`-falign-*=1`) minimizan los bytes muertos entre funciones en el binario plano.

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
.bss    ALIGN(1)
/DISCARD/ : { .eh_frame, .comment, .note.* }
```

Idéntico en estructura a `modules/hello/module.ld`. El módulo se carga en la dirección virtual 0; el kernel lee el header en el offset 0 y llama a la entrada en el offset 56 (`sizeof(module_header_t)`).

---

## Cómo el Kernel Lo Lanza

`modules_launch_entry()` en `kernel/modules.c` busca en la lista de módulos uno llamado `COMMAND.KERN` (comprobado tanto en cmdline como en `module_header_t.name`). Cuando lo encuentra:

1. Imprime `"Launching COMMAND.KERN..."`.
2. Calcula la dirección de entrada: `module_start + sizeof(module_header_t)` = `module_start + 56`.
3. Castea la entrada a `module_entry_fn` y la llama con `&kernel_kst`.
4. El módulo se ejecuta; cuando retorna, `modules_launch_entry` retorna y `kernel_main` llama a `hal_panic("kernel_main end reached.")`.

Actualmente no hay mecanismo para que el shell ceda el control de vuelta al kernel de forma ordenada. Un retorno desde `module_main` se considera un error.

---

## ABI Utilizada

`command_kern` usa `module_abi.h` directamente (sin CRT, sin newlib). La función de entrada sigue el ABI de módulo bare:

```c
typedef void (*module_entry_fn)(const kst_t* kst);
```

Para usar `printf`, `malloc` u otras funciones libc, el módulo tendría que ser reconstruido con el CRT + newlib de `newlib/`. Ver `doc/newlib/newlib_es.md` para ver cómo hacerlo.

---

## Configuración de GRUB

El Makefile raíz genera automáticamente `grub.cfg` durante `make iso`. Por cada archivo `.bin` en `build/x86_64/iso/modules/`, añade:

```
module2 /modules/command.kern.bin command.kern.bin
```

GRUB pasa el nombre de archivo del módulo como su línea de comandos. El kernel lo almacena en `module_t.cmdline`. El módulo también se identifica por `module_header_t.name = "COMMAND.KERN"`.

---

## Funcionalidad Planificada

A medida que KDOS evolucione, `command_kern` está pensado para convertirse en un intérprete de comandos interactivo completo con:

- Lectura de teclas pulsadas mediante `kst->console.getchar()` (actualmente un stub que devuelve -1, a la espera del driver de teclado)
- Parseo y despacho de comandos integrados
- Carga y ejecución de módulos adicionales
- Una interfaz básica de sistema de archivos una vez que se implementen los drivers de almacenamiento

Esto convierte a `command_kern` en el lugar principal donde se ejercerá la funcionalidad futura del SO.
