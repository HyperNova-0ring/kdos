ARCH    ?= x86_64
VERBOSE ?= 0

ifeq ($(VERBOSE), 0)
    Q = @
else
    Q =
endif

CC      = $(ARCH)-elf-gcc
LD      = $(ARCH)-elf-ld
OBJCOPY = $(ARCH)-elf-objcopy

OUTDIR = build/$(ARCH)

CFLAGS = -ffreestanding -mno-red-zone -fno-stack-protector \
         -mno-sse -mno-sse2 -mno-mmx \
         -fPIC -O2 \
         -falign-functions=1 -falign-loops=1 -falign-jumps=1 \
         -I../kernel

TARGET_BIN = $(OUTDIR)/command.kern.bin
TARGET_ELF = $(OUTDIR)/command.kern.elf

# main.o must be first: the linker script places .module_header and
# .text.entry (both in main.c) at the binary's very beginning.
OBJS = $(OUTDIR)/main.o \
       $(OUTDIR)/transfer.o \
       $(OUTDIR)/recover.o

# ─── Rules ──────────────────────────────────────────────────────

all: $(TARGET_BIN)

$(OUTDIR)/%.o: %.c
	$(Q)mkdir -p $(OUTDIR)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]  $<"

$(OUTDIR)/%.o: %.S
	$(Q)mkdir -p $(OUTDIR)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "[AS]  $<"

$(TARGET_ELF): $(OBJS)
	$(Q)$(LD) -T module.ld -nostdlib $(OBJS) -o $@
	@echo "[LD]  $@"

$(TARGET_BIN): $(TARGET_ELF)
	$(Q)$(OBJCOPY) -O binary $< $@
	@echo "[BIN] $@"

clean:
	$(Q)rm -rf build/
	@echo "[CLEAN] command_kern/build/"

.PHONY: all clean
