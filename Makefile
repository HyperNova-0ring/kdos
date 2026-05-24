ARCH    ?= x86_64
VERBOSE ?= 0
CONSOLE ?= vga

ifeq ($(VERBOSE), 0)
    Q = @
else
    Q =
endif

OUTDIR = build/$(ARCH)

KERNEL_DIR       = kernel
NEWLIB_DIR       = newlib
COMMAND_KERN_DIR = command_kern

# ─── Top-level targets ──────────────────────────────────────────

.PHONY: all iso run clean \
        _build-kernel _build-newlib _build-command_kern \
        _copy-kernel  _copy-newlib  _copy-command_kern

all: _copy-kernel _copy-newlib _copy-command_kern

iso: all
	$(Q)mkdir -p $(OUTDIR)/iso/boot/grub $(OUTDIR)/iso/modules
	$(Q)cp $(OUTDIR)/kernel.elf $(OUTDIR)/iso/boot/kernel.elf
	$(Q)cp $(OUTDIR)/modules/*.bin $(OUTDIR)/iso/modules/ 2>/dev/null || true
	$(Q){ \
	    echo 'set timeout=3'; \
	    echo 'set default=0'; \
	    echo 'menuentry "KDOS ($(ARCH))" {'; \
	    echo '    insmod multiboot2'; \
	    echo "    multiboot2 /boot/kernel.elf console=$(CONSOLE)"; \
	    for b in $(OUTDIR)/iso/modules/*.bin; do \
	        [ -f "$$b" ] || continue; \
	        n=$$(basename $$b); \
	        echo "    module2 /modules/$$n $$n"; \
	    done; \
	    echo '    boot'; \
	    echo '}'; \
	} > $(OUTDIR)/iso/boot/grub/grub.cfg
	$(Q)grub-mkrescue -o $(OUTDIR)/kernel-$(ARCH).iso $(OUTDIR)/iso/ 2>/dev/null
	@echo "[ISO] $(OUTDIR)/kernel-$(ARCH).iso"

run: iso
	$(Q)qemu-system-$(ARCH) \
	    -cdrom $(OUTDIR)/kernel-$(ARCH).iso \
	    -m 128M \
	    -serial stdio \
	    -serial tcp::4444,server,nowait \
	    -no-reboot \
	    2>/dev/null

clean:
	$(Q)$(MAKE) -C $(KERNEL_DIR)       clean ARCH=$(ARCH)
	$(Q)$(MAKE) -C $(NEWLIB_DIR)       clean ARCH=$(ARCH)
	$(Q)$(MAKE) -C $(COMMAND_KERN_DIR) clean ARCH=$(ARCH)
	$(Q)rm -rf build/
	@echo "[CLEAN] build/"

# ─── Sub-project builds ─────────────────────────────────────────

_build-kernel:
	$(Q)$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) VERBOSE=$(VERBOSE)

_build-newlib:
	$(Q)$(MAKE) -C $(NEWLIB_DIR) ARCH=$(ARCH) VERBOSE=$(VERBOSE)

_build-command_kern:
	$(Q)$(MAKE) -C $(COMMAND_KERN_DIR) ARCH=$(ARCH) VERBOSE=$(VERBOSE)

# ─── Artifact copy ──────────────────────────────────────────────

_copy-kernel: _build-kernel
	$(Q)mkdir -p $(OUTDIR)/modules
	$(Q)cp $(KERNEL_DIR)/build/kernel-$(ARCH).elf    $(OUTDIR)/kernel.elf
	$(Q)cp $(KERNEL_DIR)/build/modules/*.bin         $(OUTDIR)/modules/ 2>/dev/null || true
	@echo "[COPY] kernel → $(OUTDIR)/"

_copy-newlib: _build-newlib
	$(Q)mkdir -p $(OUTDIR)
	$(Q)cp $(NEWLIB_DIR)/build/$(ARCH)/crt.o         $(OUTDIR)/
	$(Q)cp $(NEWLIB_DIR)/build/$(ARCH)/libnewlib.a   $(OUTDIR)/
	$(Q)cp $(NEWLIB_DIR)/build/$(ARCH)/prog.ld     $(OUTDIR)/
	@echo "[COPY] newlib → $(OUTDIR)/"

_copy-command_kern: _build-command_kern
	$(Q)mkdir -p $(OUTDIR)/modules
	$(Q)cp $(COMMAND_KERN_DIR)/build/$(ARCH)/command.kern.bin $(OUTDIR)/modules/
	@echo "[COPY] command_kern → $(OUTDIR)/modules/"
