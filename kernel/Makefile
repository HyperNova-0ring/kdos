# Verbose: mostrar comandos completos solo si VERBOSE=1
VERBOSE ?= 0
ifeq ($(VERBOSE), 0)
    Q = @
else
    Q =
endif

# Arquitectura por defecto
ARCH ?= x86_64

# Toolchain según arquitectura
CC  = $(ARCH)-elf-gcc
LD  = $(ARCH)-elf-ld
OBJCOPY = $(ARCH)-elf-objcopy

# Directorios
ARCH_DIR = arch/$(ARCH)
OBJ_DIR  = build/$(ARCH)
MOD_DIR  = modules

# Flags
CFLAGS  = -ffreestanding -mno-red-zone -fno-stack-protector \
          -mno-sse -mno-sse2 -mno-mmx \
          -I$(ARCH_DIR) -I. \
          -O2

LDFLAGS = -T $(ARCH_DIR)/link.ld -nostdlib

# Fuentes
C_COMMON_SRCS = kernel.c modules.c
C_ARCH_SRCS   = $(wildcard $(ARCH_DIR)/*.c)
S_ARCH_SRCS   = $(wildcard $(ARCH_DIR)/*.S)

# Objetos
OBJS  = $(patsubst %.c,                 $(OBJ_DIR)/%.o,      $(C_COMMON_SRCS))
OBJS += $(patsubst $(ARCH_DIR)/%.c,     $(OBJ_DIR)/arch/%.o, $(C_ARCH_SRCS))
OBJS += $(patsubst $(ARCH_DIR)/%.S,     $(OBJ_DIR)/arch/%.o, $(S_ARCH_SRCS))

MODULE_DIRS = $(wildcard $(MOD_DIR)/*)
MODULE_BINS = $(foreach d, $(MODULE_DIRS), \
              build/modules/$(notdir $(d)).bin)

TARGET = build/kernel-$(ARCH).elf

# ─── REGLAS ────────────────────────────────────────────────

all: $(TARGET) $(MODULE_BINS)

$(TARGET): $(OBJS)
	$(Q)mkdir -p build
	$(Q)$(LD) $(LDFLAGS) -o $@ $^
	@echo "[LD]  $@"

$(OBJ_DIR)/%.o: %.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]  $<"

$(OBJ_DIR)/arch/%.o: $(ARCH_DIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]  $<"

$(OBJ_DIR)/arch/%.o: $(ARCH_DIR)/%.S
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -c $< -o $@
	@echo "[AS]  $<"

# ─── MODULES ───────────────────────────────────────────────

MOD_CFLAGS = $(CFLAGS) -falign-functions=1 -falign-loops=1 -falign-jumps=1 -fPIC

build/modules/%.bin:
	$(Q)mkdir -p build/modules
	$(Q)$(CC) $(MOD_CFLAGS) -I. \
	    -c $(wildcard $(MOD_DIR)/$*/*.c) \
	    -o build/modules/$*.o
	$(Q)$(LD) -T $(MOD_DIR)/$*/module.ld -nostdlib \
	    -o build/modules/$*.elf \
	    build/modules/$*.o
	$(Q)$(OBJCOPY) -O binary \
	    build/modules/$*.elf $@
	@echo "[MOD] $@"

# ─── ISO y QEMU ────────────────────────────────────────────

iso: all
	$(Q)mkdir -p iso/boot/grub iso/modules
	$(Q)cp $(TARGET) iso/boot/kernel.elf
	$(Q)cp build/modules/*.bin iso/modules/ 2>/dev/null || true
	$(Q)printf \
	'set timeout=3\nset default=0\n\
	menuentry "DOS-like Kernel ($(ARCH))" {\n\
    insmod multiboot2\n\
    multiboot2 /boot/kernel.elf\n\
    module2 /modules/hello.bin hello.bin\n\
    boot\n\
	}\n' > iso/boot/grub/grub.cfg
	$(Q)grub-mkrescue -o build/kernel-$(ARCH).iso iso/ 2>/dev/null
	@echo "[ISO] build/kernel-$(ARCH).iso"

run: iso
	$(Q)qemu-system-$(ARCH) \
	    -cdrom build/kernel-$(ARCH).iso \
	    -m 128M \
	    -serial stdio \
	    -no-reboot \
	    2>/dev/null

clean:
	$(Q)rm -rf build/ iso/
	@echo "[CLEAN] build/ iso/"

.PHONY: all iso run clean