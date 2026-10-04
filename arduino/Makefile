ROOT := $(abspath .)
CLI := $(ROOT)/.tools/bin/arduino-cli
CONFIG := $(ROOT)/arduino-cli.yaml
FQBN := STMicroelectronics:stm32:GenF4:pnum=GENERIC_F405RGTX,usb=CDCgen

.PHONY: all
all:
	$(CLI) --config-file $(CONFIG) compile --fqbn $(FQBN) --build-path $(ROOT)/build/usb_heartbeat $(ROOT)/usb_heartbeat
