# AsterOS build systém

ARCH := x86_64
# Prefer dedicated cross toolchain, fallback to host toolchain when unavailable.
CROSS ?= $(shell if command -v x86_64-elf-gcc >/dev/null 2>&1; then echo x86_64-elf-; else echo; fi)
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
AS := nasm

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -Wall -Wextra -Iinclude
LDFLAGS := -nostdlib -T linker.ld

BUILD := build
IMG := $(BUILD)/aster.img
DATA_DISK := asterfs.img
BRIDGE_IF ?= br0

KERNEL_OBJS := \
	$(BUILD)/arch/x86_64/entry.o \
	$(BUILD)/arch/x86_64/gdt.o \
	$(BUILD)/arch/x86_64/idt.o \
	$(BUILD)/arch/x86_64/interrupts.o \
	$(BUILD)/arch/x86_64/cpu.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/panic.o \
	$(BUILD)/kernel/printk.o \
	$(BUILD)/kernel/memory.o \
	$(BUILD)/kernel/process.o \
	$(BUILD)/kernel/scheduler.o \
	$(BUILD)/kernel/syscall.o \
	$(BUILD)/kernel/aster_api.o \
	$(BUILD)/kernel/string.o \
	$(BUILD)/drivers/display.o \
	$(BUILD)/drivers/keyboard.o \
	$(BUILD)/drivers/serial.o \
	$(BUILD)/drivers/timer.o \
	$(BUILD)/drivers/storage.o \
	$(BUILD)/apps/calc_app.o \
	$(BUILD)/apps/tetris_app.o

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)/boot $(BUILD)/kernel $(BUILD)/arch/x86_64 $(BUILD)/drivers $(BUILD)/apps

$(BUILD)/boot/boot.bin: boot/boot.asm | $(BUILD)
	$(AS) -f bin $< -o $@

$(BUILD)/boot/stage2.bin: boot/stage2.asm boot/disk.asm | $(BUILD)
	$(AS) -I boot/ -f bin $< -o $@

$(BUILD)/arch/x86_64/%.o: arch/x86_64/%.asm | $(BUILD)
	$(AS) -f elf64 $< -o $@

$(BUILD)/arch/x86_64/%.o: arch/x86_64/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/drivers/%.o: drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/apps/%.o: apps/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(IMG): $(BUILD)/boot/boot.bin $(BUILD)/boot/stage2.bin $(BUILD)/kernel.bin
	dd if=/dev/zero of=$(IMG) bs=512 count=2880
	dd if=$(BUILD)/boot/boot.bin of=$(IMG) conv=notrunc
	dd if=$(BUILD)/boot/stage2.bin of=$(IMG) bs=512 seek=1 conv=notrunc
	dd if=$(BUILD)/kernel.bin of=$(IMG) bs=512 seek=17 conv=notrunc

$(DATA_DISK):
	if [ -f $(BUILD)/asterfs.img ] && [ ! -f $(DATA_DISK) ]; then cp $(BUILD)/asterfs.img $(DATA_DISK); fi
	test -f $(DATA_DISK) || dd if=/dev/zero of=$(DATA_DISK) bs=512 count=8192

run: $(IMG) $(DATA_DISK)
	qemu-system-x86_64 -no-reboot -no-shutdown -drive format=raw,file=$(IMG),if=ide,index=0 -drive format=raw,file=$(DATA_DISK),if=ide,index=1

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
