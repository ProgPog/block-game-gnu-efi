#!/bin/bash
#x86_64-w64-mingw32-gcc -ffreestanding -L/home/minty/OS/gnu-efi/gnu-efi/lib -I/home/minty/OS/gnu-efi/gnu-efi/inc -I/home/minty/OS/gnu-efi/gnu-efi/x86_64 -I/home/minty/OS/gnu-efi/gnu-efi/inc/protocol -c -o hello.o hello.c
#x86_64-w64-mingw32-gcc -ffreestanding -L/home/minty/OS/gnu-efi/gnu-efi/lib -I/home/minty/OS/gnu-efi/gnu-efi/inc -I/home/minty/OS/gnu-efi/gnu-efi/x86_64 -I/home/minty/OS/gnu-efi/gnu-efi/inc/protocol -c -o data.o gnu-efi/gnu-efi/lib/data.c

#x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o BOOTX64.EFI hello.o data.o

#dd if=/dev/zero of=fat.img bs=1k count=1440
#mformat -i fat.img -f 1440 ::
dd if=/dev/zero of=fat.img bs=1k count=8000
mformat -i fat.img ::
mmd -i fat.img ::/EFI
mmd -i fat.img ::/IMG
mcopy -i fat.img ./img/BLOCKDEF.bmp ::/IMG
mcopy -i fat.img ./img/BACK.bmp ::/IMG
mcopy -i fat.img ./img/FONT0.bmp ::/IMG
mcopy -i fat.img ./img/FONT1.bmp ::/IMG
mcopy -i fat.img ./img/FONT2.bmp ::/IMG
mcopy -i fat.img ./img/FONT3.bmp ::/IMG
mcopy -i fat.img ./img/FONT4.bmp ::/IMG
mcopy -i fat.img ./img/FONT5.bmp ::/IMG
mcopy -i fat.img ./img/FONT6.bmp ::/IMG
mcopy -i fat.img ./img/FONT7.bmp ::/IMG
mcopy -i fat.img ./img/FONT8.bmp ::/IMG
mcopy -i fat.img ./img/FONT9.bmp ::/IMG
mcopy -i fat.img ./img/BACKGROUND.bmp ::/IMG
mcopy -i fat.img ./img/SCORE_BACKGROUND.bmp ::/IMG
mcopy -i fat.img ./img/HELP.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK1.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK2.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK3.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK4.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK5.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK6.bmp ::/IMG
mcopy -i fat.img ./img/BLOCK7.bmp ::/IMG
mcopy -i fat.img ./img/GAME_OVER.bmp ::/IMG
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img BOOTX64.efi ::/EFI/BOOT

#mkgpt -o hdimage.bin --image-size 4096 --part fat.img --type system
mkgpt -o hdimage.bin --image-size 16384 --part fat.img --type system
qemu-system-x86_64 -L . -pflash OVMF.fd -hda hdimage.bin
