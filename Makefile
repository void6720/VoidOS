# VoidOS build
#
#   make            build build/os-image.bin (bootable floppy image)
#   make run        boot it in QEMU (KVM if /dev/kvm is available)
#   make test       headless boot test: boots QEMU, greps the serial log
#   make rootfs     show the staged root filesystem (/) tree
#   make clean

NASM   ?= nasm
CC     ?= gcc
LD     ?= ld
QEMU   ?= qemu-system-x86_64

CFLAGS  = -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib \
          -O2 -Wall -Wextra -Ikernel
LDFLAGS = -m elf_i386 -T kernel/linker.ld

BUILD   = build
C_SRC   = kernel/kernel.c kernel/vga.c kernel/serial.c kernel/memory.c \
          kernel/interrupts.c kernel/keyboard.c kernel/mouse.c \
          kernel/fb.c kernel/gui.c
C_OBJ   = $(C_SRC:kernel/%.c=$(BUILD)/%.o)

IMAGE   = $(BUILD)/os-image.bin

all: $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/isr.o: kernel/isr.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

kernel/font.h: tools/make_font.py
	python3 tools/make_font.py

$(BUILD)/fb.o $(BUILD)/gui.o: kernel/font.h

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(BUILD)/isr.o $(C_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@

# Bootable 1.44 MB floppy image: boot sector + kernel, zero-padded.
$(IMAGE): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 1474560 $@
	@echo "Built $@ ($$(stat -c%s $@) bytes)"

run: $(IMAGE)
	./run.sh

test: $(IMAGE)
	@echo "Booting headless for 6s, capturing serial output..."
	-timeout 6 $(QEMU) -fda $(IMAGE) -boot order=a -display none \
		-serial file:$(BUILD)/serial.log -monitor none
	@grep -q "VoidOS" $(BUILD)/serial.log \
		&& echo "BOOT TEST PASSED - serial log:" \
		|| (echo "BOOT TEST FAILED"; cat $(BUILD)/serial.log; exit 1)
	@cat $(BUILD)/serial.log

rootfs:
	@find rootfs -type d | sort
	@find rootfs -type f | sort

clean:
	rm -rf $(BUILD)

.PHONY: all run test rootfs clean
