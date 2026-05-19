<div align="center"> <img src="gimmel-os-logo.png" alt="GimmelOS" width="100%"> <b>A lightweight and useless OS on C and ASM<br></b> </div> 

# Quick Start

## Build

```sh
./build.sh
```

or

```sh
make clean
make all 2>&1
grub-mkrescue -o iso/gimmelos.iso iso/
```

## Disk setup (required)

Create virtual disk:
```sh
qemu-img create -f raw disk.img 64M
```

## Run OS

```sh
qemu-system-i386 -cdrom build/gimmelos.iso -hda disk.img -m 32M
```