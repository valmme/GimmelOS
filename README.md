# Disk setup (required)

Create virtual disk:
```sh
qemu-img create -f raw disk.img 64M
````

# Run OS

```sh
qemu-system-i386 -cdrom build/gimmelos.iso -hda disk.img -m 32M
```