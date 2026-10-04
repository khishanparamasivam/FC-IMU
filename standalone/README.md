# STM32F405 compile-only baseline

Build on Ubuntu:

```sh
cd /home/khishan/IMU/standalone
make
```

Requires `make` and the `arm-none-eabi-gcc` toolchain. No dependencies are downloaded.
The Makefile has no flashing target.

This is a bare-metal C compiler/linker smoke test for the STM32F405RGT6,
not an Arduino sketch or an integration of the finani ICM42688 library.
It provides a core vector table, initializes C data and BSS, and enters
an infinite loop with interrupts disabled. It does not configure USB,
SPI, the IMU, or external clocks. Peripheral interrupt vectors must be
added before enabling peripheral interrupts.

`build/minimal.elf` is the linked application with debug information;
`build/minimal.map` describes its memory layout. Successful compilation
does not establish that the application works on the board.

The linker reserves 1 MiB flash and 128 KiB ordinary SRAM; the separate
64 KiB CCM RAM is unused. Hardware reference:
https://www.st.com/resource/en/datasheet/stm32f405rg.pdf

Before any custom firmware flash, preserve the existing firmware and
configuration and establish recovery via verified BOOT0 access or SWD.
The next project choice is an Arduino-compatible framework for the
finani library, or a bare-metal sensor driver; this baseline does not
settle that choice.
