make clean
make all 2>&1
echo "=== Build OK ==="
grub-mkrescue -o iso/gimmelos.iso iso/