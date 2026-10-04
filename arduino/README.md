# MAMBA USB serial compile check

This is the earlier Arduino CLI reference. Ongoing development uses the
PlatformIO project in the parent directory; run `make` there.

From this directory, run `make`. This only compiles; there is no upload target.
The sketch prints a USB serial heartbeat once per second when a host terminal
is connected. No sensor code or motor configuration is included yet.

Installed locally for this check: Arduino CLI 1.5.2-rc.1 and official STM32duino
core 3.0.0, with its packaged compiler. The downloaded CLI turned out to be
a release candidate; use a pinned stable CLI for a maintained project.
All downloaded packages live in `.tools/`.

The FQBN selects Generic F405RGTx with USB CDC on generic `Serial`.
`mamba_clock.c` overrides the generic variant's internal-oscillator clock
with the handoff's 8 MHz external crystal, producing 168 MHz CPU and 48 MHz
USB clocks. `build_opt.h` sets HSE_VALUE consistently across the core.
The generic core maps USB FS to PA11/PA12 and disables VBUS detection by
default. These settings compile, but actual MAMBA USB operation has not
been tested. The clock source and USB wiring still require hardware validation.

Nothing has been flashed. Confirm a recovery route before uploading custom
firmware. Existing Betaflight settings are in `../bf-backup.txt`; that file
is not a firmware binary backup.

PlatformIO is the recommended maintained project workflow, with its Arduino
framework using the official STM32duino core. This sketch and clock source
can be migrated; USB flags and HSE_VALUE must be carried into platformio.ini.

References:
- https://github.com/stm32duino/Arduino_Core_STM32
- https://docs.platformio.org/en/latest/boards/ststm32/genericSTM32F405RG.html
- https://docs.platformio.org/en/latest/platforms/ststm32.html
