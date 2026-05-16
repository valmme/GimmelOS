CC      = gcc
AS      = nasm
LD      = ld

GCC_INC := $(shell gcc -m32 -print-file-name=include)
CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-pic \
          -nostdlib -nostdinc -Wall -Wextra -O2 \
          -std=c11 -Isrc -I$(GCC_INC)
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

SRC_DIR = src
OBJ_DIR = build

C_SRCS  = $(wildcard $(SRC_DIR)/*.c)
AS_SRCS = $(wildcard $(SRC_DIR)/*.asm)
C_OBJS  = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
AS_OBJS = $(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.asm.o, $(AS_SRCS))

KERNEL  = iso/boot/gimmelos.bin
ISO     = iso/gimmelos.iso

.PHONY: all clean run iso

all: $(ISO)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.asm.o: $(SRC_DIR)/%.asm | $(OBJ_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KERNEL): $(OBJ_DIR)/boot.asm.o $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(ISO): $(KERNEL)
	grub-mkrescue -o $(ISO) iso/ 2>/dev/null || \
	grub2-mkrescue -o $(ISO) iso/

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 32M

run-headless: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 32M -nographic \
	  -serial mon:stdio -display none

clean:
	rm -rf $(OBJ_DIR) $(KERNEL) $(ISO)