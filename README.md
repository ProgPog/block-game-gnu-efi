# Block Game GNU-EFI (UEFI)

An UEFI app created for my System Software university classes.

# Game Screenshot

![game screenshot](https://github.com/user-attachments/assets/24436ade-a89f-4149-b900-5c0ef304717d)

# Running

To run the compile.sh script you need the following dependecies:
* A standard set of tools such as gcc, make etc.
* [GNU-EFI](https://sourceforge.net/projects/gnu-efi/) installed and compiled in the following directory ROOT/gnu-efi/gnu-efi/(Makefile and other files here)

To run the run.sh script you additionally need the following dependecies:
* Mtools, qemu-system-x86_64
* [Mkgpt](https://github.com/jncronin/mkgpt)
* OVMF.fd at the root of the project directory

Font used in bmp font files (FONTX.bmp) is RobotoMono.
