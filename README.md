# Quick Start
```sh
qemu-system-i386 -cdrom iso/gimmelos.iso -m 32M
```

# Build
```sh
./build.sh
```
or
```sh
make clean
make all 2>&1
grub-mkrescue -o iso/gimmelos.iso iso/
```