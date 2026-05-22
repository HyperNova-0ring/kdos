# Verbose: show full commands only when VERBOSE=1
VERBOSE ?= 0
ifeq ($(VERBOSE), 0)
    Q = @
else
    Q =
endif

# Default architecture
ARCH  ?= x86_64
DEBUG ?= 0

# Toolchain for the target architecture
CC  = $(ARCH)-elf-gcc
LD  = $(ARCH)-elf-ld
OBJCOPY = $(ARCH)-elf-objcopy

# Directories
ARCH_DIR      = arch/$(ARCH)
OBJ_DIR       = build/$(ARCH)
MOD_DIR       = modules
ARCH_MOD_DIR  = $(ARCH_DIR)/modules
DEBUG_MOD_DIR = $(ARCH_MOD_DIR)/debug

# Flags
CFLAGS  = -ffreestanding -mno-red-zone -fno-stack-protector \
          -mno-sse -mno-sse2 -mno-mmx \
          -I$(ARCH_DIR) -I. \
          -O2

LDFLAGS = -T $(ARCH_DIR)/link.ld -nostdlib

# Sources
C_COMMON_SRCS = kernel.c modules.c
C_ARCH_SRCS   = $(wildcard $(ARCH_DIR)/*.c)
S_ARCH_SRCS   = $(wildcard $(ARCH_DIR)/*.S)

# Objects
OBJS  = $(patsubst %.c,                 $(OBJ_DIR)/%.o,      $(C_COMMON_SRCS))
OBJS += $(patsubst $(ARCH_DIR)/%.c,     $(OBJ_DIR)/arch/%.o, $(C_ARCH_SRCS))
OBJS += $(patsubst $(ARCH_DIR)/%.S,     $(OBJ_DIR)/arch/%.o, $(S_ARCH_SRCS))

MODULE_DIRS  = $(wildcard $(MOD_DIR)/*)
MODULE_DIRS += $(filter-out $(DEBUG_MOD_DIR), $(wildcard $(ARCH_MOD_DIR)/*))
ifeq ($(DEBUG), 1)
    MODULE_DIRS += $(wildcard $(DEBUG_MOD_DIR)/*)
    CFLAGS      += -D__DEBUG__
endif

MODULE_BINS = $(foreach d, $(MODULE_DIRS), \
              build/modules/$(notdir $(d)).bin)

TARGET = build/kernel-$(ARCH).elf

# ─── RULES ─────────────────────────────────────────────────

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
	$(Q)msrc=$$(for d in "$(MOD_DIR)/$*" "$(ARCH_MOD_DIR)/$*" "$(DEBUG_MOD_DIR)/$*"; do \
	                [ -d "$$d" ] && { echo "$$d"; break; }; \
	            done); \
	    $(CC) $(MOD_CFLAGS) -I. -c $$msrc/*.c -o build/modules/$*.o; \
	    $(LD) -T $$msrc/module.ld -nostdlib \
	        -o build/modules/$*.elf build/modules/$*.o; \
	    $(OBJCOPY) -O binary build/modules/$*.elf $@
	@echo "[MOD] $@"

# ─── ISO and QEMU ──────────────────────────────────────────

iso: all
	$(Q)mkdir -p iso/boot/grub iso/modules
	$(Q)cp $(TARGET) iso/boot/kernel.elf
	$(Q)cp build/modules/*.bin iso/modules/ 2>/dev/null || true
	$(Q){ \
	    echo 'set timeout=3'; \
	    echo 'set default=0'; \
	    echo 'menuentry "KDOS ($(ARCH))" {'; \
	    echo '    insmod multiboot2'; \
	    echo '    multiboot2 /boot/kernel.elf'; \
	    for b in iso/modules/*.bin; do \
	        n=$$(basename $$b); \
	        echo "    module2 /modules/$$n $$n"; \
	    done; \
	    echo '    boot'; \
	    echo '}'; \
	} > iso/boot/grub/grub.cfg
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