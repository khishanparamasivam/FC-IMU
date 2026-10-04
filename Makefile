PROJECT_ROOT := $(abspath .)
PIO := $(PROJECT_ROOT)/.tools/platformio/penv/bin/pio
export PLATFORMIO_CORE_DIR := $(PROJECT_ROOT)/.tools/platformio

.PHONY: all
all:
	$(PIO) run --project-dir $(PROJECT_ROOT) -e mamba_usb
