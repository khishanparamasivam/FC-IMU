# MAMBA STM32F405 firmware

For a detailed account of the setup, commands, backups, recovery, flashing,
and hardware verification, read the
[USB heartbeat guide](docs/MAMBA_F405_USB_HEARTBEAT_GUIDE.md).

The active project is now PlatformIO with the official STM32duino Arduino
framework. The default `mamba_usb` build is the USB serial heartbeat,
already flashed and verified on the FC. A separate `mamba_spi` build adds
the finani ICM42688 library; it has not yet been verified on hardware.

## Next bench test: IMU over SPI

`src/imu_spi.cpp` adapts the upstream Basic_SPI example to SPI1:
SCK PA5, MISO PA6, MOSI PA7, and CS PA4. The library revision is pinned
in `platformio.ini`. Data transfers start conservatively at 1 MHz.
The handoff's CW180 orientation reverses X and Y for both sensors;
Z is unchanged. Output is acceleration in g, gyro rate in degrees/second,
and temperature in Celsius, about ten lines/second. This test uses polling;
it does not yet use DRDY, FIFO, or motor outputs.

Build without flashing:

```sh
.tools/platformio/penv/bin/pio run -e mamba_spi
```

Enter DFU using the side button while connecting USB, release the button,
then upload:

```sh
sudo .tools/platformio/penv/bin/pio run -e mamba_spi -t upload
```

Both environments use `upload_protocol = dfu`. PlatformIO invokes dfu-util
to write the selected binary at 0x08000000 and leave the bootloader.
After upload, release the button and reconnect USB normally if necessary.
With the battery disconnected, place the board still and open:

```sh
picocom -b 115200 /dev/ttyACM0
```

The SPI firmware waits for a serial monitor, announces gyro calibration,
then initializes the sensor. Keep the board still until sample rows appear.
Successful initialization reports `IMU.begin status=1`. On a stationary
board, acceleration magnitude should be roughly 1 g and gyro rates near
zero; tilting/rotating should change the readings. Initialization failures
print a repeated negative status; -3 means the library's WHO_AM_I check
failed. Exit picocom with Ctrl+A, then Ctrl+X.

The heartbeat guide below records the earlier completed hardware test.
The SPI build and upload configuration are newer additions; compiling
successfully does not establish that the sensor works on this board.

## Build on this machine

```sh
cd /home/khishan/IMU
make
```

`make` invokes the locally installed PlatformIO Core and builds the
`mamba_usb` environment. It does not upload firmware. Output files are
`.pio/build/mamba_usb/firmware.elf` and `firmware.bin`.

The equivalent direct PlatformIO command is:

```sh
PLATFORMIO_CORE_DIR="$PWD/.tools/platformio" .tools/platformio/penv/bin/pio run -e mamba_usb
```

PlatformIO and its packages are local to `.tools/platformio/`. No activation
or global Python installation is required. First-time package installation
requires internet access; subsequent builds use the local packages.
The installed Core version is 6.2.0, recorded in `requirements-platformio.txt`.

For a fresh machine, install PlatformIO Core 6.2.0 in an isolated environment
at `.tools/platformio/penv`, or install it through PlatformIO's official
installer and use that installation's `pio run -e mamba_usb` command.
The Makefile expects the local environment path used on this machine.

## Configuration and source

- `platformio.ini`: STM32 platform 20.0.0, STM32duino package 4.30000.0
  (upstream core 3.0.0), Generic STM32F405RG board, USB CDC, and 8 MHz HSE.
- `src/main.cpp`: USB heartbeat once per second while a host terminal is
  connected. There is no wait for a terminal at startup.
- `src/mamba_clock.c`: override of the framework's weak clock function,
  using the handoff's external crystal for 168 MHz CPU and 48 MHz USB.

Generic STM32F405RG identifies the MCU, not all MAMBA board peripherals.
USB enumeration and heartbeat output have been verified. Absolute clock
frequency has not been measured. USB CDC uses `Serial`;
115200 is the terminal setting, not a USB throughput limit. The firmware
does not configure motors, SPI, or the IMU, and does not implement a
Betaflight-style `bl` recovery command.

## Verification

The PlatformIO build completed successfully with GCC 12.3.1, STM32duino
3.0.0, CMSIS 6.3.0, and CMSIS DSP 1.16.2. Reported usage was 19,900 bytes
of flash and 4,628 bytes of RAM. Both ELF and binary outputs were generated.
ELF symbol and disassembly inspection confirmed USB CDC/SerialUSB code and
the application's strong `SystemClock_Config` with the intended HSE PLL
settings. The isolated Python environment passed `pip check`.
On 2026-10-04, hardware-button DFU entry was verified as USB 0483:df11.
The original 1 MiB main flash was saved in
`backups/betaflight-original-flash.bin`, with its checksum in `SHA256SUMS`.
The user flashed the 20,324-byte heartbeat binary using dfu-util. It
enumerated as USB 0483:5740 on `/dev/ttyACM0`. A read from that port returned:

```text
MAMBA STM32F405 USB heartbeat; uptime_ms=36391
MAMBA STM32F405 USB heartbeat; uptime_ms=37401
MAMBA STM32F405 USB heartbeat; uptime_ms=38411
```

The serial port was closed after the check. To observe the heartbeat:

```sh
picocom -b 115200 /dev/ttyACM0
```

Exit picocom with Ctrl+A, then Ctrl+X. The FC is now running the heartbeat
firmware, not Betaflight. To enter DFU for further flashing, disconnect USB,
hold the side button's actuator inward while reconnecting USB, then release
it. Keep the battery disconnected for these bench checks.
`bf-backup.txt` contains Betaflight settings; restoration of the saved
firmware has not been tested.

## Earlier build checks

`standalone/` keeps the earlier bare-metal GCC smoke test. `arduino/` keeps
the earlier Arduino CLI build as a reference; use the root project for
ongoing development.

## Upstream references

- https://docs.platformio.org/en/latest/boards/ststm32/genericSTM32F405RG.html
- https://docs.platformio.org/en/latest/platforms/ststm32.html
- https://github.com/platformio/platform-ststm32/tree/v20.0.0
- https://github.com/finani/ICM42688
