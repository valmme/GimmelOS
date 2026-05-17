CC      = gcc
AS      = nasm
LD      = ld

SRC_DIR = src
OBJ_DIR = build

ISO_DIR = iso
KERNEL  = $(ISO_DIR)/boot/gimmelos.bin
ISO     = build/gimmelos.iso

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-pic \
          -nostdlib -nostdinc -Wall -Wextra -O2 -std=c11 -Isrc \
		  -Wno-misleading-indentation -Wno-implicit-function-declaration -Wno-int-conversion

ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

C_SRCS  = $(shell find $(SRC_DIR) -name "*.c")
AS_SRCS = $(shell find $(SRC_DIR) -name "*.asm")

C_OBJS  = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
AS_OBJS = $(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.asm.o, $(AS_SRCS))

OBJS = $(AS_OBJS) $(C_OBJS)

.PHONY: all clean run

all: $(ISO)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(ISO_DIR)/boot:
	mkdir -p $(ISO_DIR)/boot

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.asm.o: $(SRC_DIR)/%.asm | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) | $(ISO_DIR)/boot
	$(LD) $(LDFLAGS) -o $@ $^

$(ISO): $(KERNEL)
	mkdir -p build
	grub-mkrescue -o $(ISO) $(ISO_DIR)/ 2>/dev/null || \
	grub2-mkrescue -o $(ISO) $(ISO_DIR)/

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 32M

clean:
	rm -rf $(OBJ_DIR) $(ISO) $(KERNEL)