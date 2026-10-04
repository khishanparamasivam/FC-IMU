CC := arm-none-eabi-gcc
SIZE := arm-none-eabi-size
CFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Os -g3 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wa,--noexecstack -Wall -Wextra -Werror
LDFLAGS := -nostdlib -T stm32f405.ld -Wl,--gc-sections,-Map=build/minimal.map -Wl,--build-id=none

.PHONY: all
all: build/minimal.elf

build:
	mkdir -p build

build/main.o: main.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/startup.o: startup.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/minimal.elf: build/startup.o build/main.o stm32f405.ld
	$(CC) $(CFLAGS) $(LDFLAGS) build/startup.o build/main.o -o $@
	$(SIZE) $@
