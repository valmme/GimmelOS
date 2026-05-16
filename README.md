<div align="center">

<img src="gimmel-os-logo.png" alt="QuarkCore" width="100%">
<b>A lightweight and useless OS on C and ASM<br></b>

</div>

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