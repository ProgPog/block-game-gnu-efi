#!/bin/bash
gcc -Ignu-efi/gnu-efi/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -Wall -Wunused -O2 -c main.c -o main.o
ld -shared -Bsymbolic -Lgnu-efi/gnu-efi/x86_64/lib -Lgnu-efi/gnu-efi/x86_64/gnuefi -Tgnu-efi/gnu-efi/gnuefi/elf_x86_64_efi.lds gnu-efi/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o main.o -o main.so -lgnuefi -lefi
objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 main.so BOOTX64.efi
