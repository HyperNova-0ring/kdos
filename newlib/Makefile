ARCH    ?= x86_64
VERBOSE ?= 0

ifeq ($(VERBOSE), 0)
    Q = @
else
    Q =
endif

CC     = $(ARCH)-elf-gcc
AR     = $(ARCH)-elf-ar
NEWLIB = /usr/local/$(ARCH)-elf

OUTDIR = build/$(ARCH)

CFLAGS = -ffreestanding -mno-red-zone -fno-stack-protector \
         -mno-sse -mno-sse2 -mno-mmx \
         -fPIC -fvisibility=hidden -O2 \
         -I../kernel -I. \
         -isystem $(NEWLIB)/include

ARTIFACTS = $(OUTDIR)/crt.o $(OUTDIR)/libnewlib.a $(OUTDIR)/module.ld

# ─── Rules ──────────────────────────────────────────────────────

all: $(ARTIFACTS)

$(OUTDIR)/crt.o: arch/$(ARCH)/crt.S
	$(Q)mkdir -p $(OUTDIR)
	$(Q)$(CC) -c $< -o $@
	@echo "[AS]  $<"

$(OUTDIR)/libnewlib.a: $(OUTDIR)/newlib.o
	$(Q)$(AR) rcs $@ $^
	@echo "[AR]  $@"

$(OUTDIR)/newlib.o: newlib.c
	$(Q)mkdir -p $(OUTDIR)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]  $<"

$(OUTDIR)/module.ld: arch/$(ARCH)/module.ld
	$(Q)mkdir -p $(OUTDIR)
	$(Q)cp $< $@
	@echo "[CP]  $@"

clean:
	$(Q)rm -rf build/
	@echo "[CLEAN] newlib/build/"

.PHONY: all clean
