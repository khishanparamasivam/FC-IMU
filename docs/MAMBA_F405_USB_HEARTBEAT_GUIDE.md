# MAMBA F405 MK2 V2: from Betaflight to a verified USB heartbeat

Session date: **4 October 2026, Asia/Singapore**  
Workspace: **`/home/khishan/IMU`**  
Host operating system: **Ubuntu**

This guide explains the work completed in this session, the commands we used, and how to build, observe, and recover the firmware. Commands that change the FC are explicitly identified. Creating this document did not perform another flash.

## 1. What we achieved

The DIATONE MAMBA F405 MK2 V2 now runs our own USB heartbeat application. We replaced its Betaflight application with firmware built using **PlatformIO and the official STM32duino Arduino framework**.

We completed the following:

1. Verified that an Arm cross-compiler was installed.
2. Built a minimal bare-metal STM32F405 application without flashing.
3. Built a USB serial example using Arduino CLI and STM32duino.
4. Migrated the active project to PlatformIO and pinned its platform/framework versions.
5. Preserved the Betaflight configuration dump.
6. Read and saved the FC's complete 1 MiB main flash before replacing its firmware.
7. Verified DFU entry using both Betaflight's software command and the physical side button.
8. Flashed the USB heartbeat.
9. Read actual heartbeat messages from `/dev/ttyACM0`.

The latest verified serial output was:

```text
MAMBA STM32F405 USB heartbeat; uptime_ms=36391
MAMBA STM32F405 USB heartbeat; uptime_ms=37401
MAMBA STM32F405 USB heartbeat; uptime_ms=38411
```

This confirms that the application starts, USB enumerates, and the host can receive its output. The ICM42688 library and sensor-reading code have **not** been integrated. The firmware does not provide flight control or configure motor outputs. Restoration of the saved Betaflight binary has **not** been tested.

## 2. Hardware and original firmware

The starting hardware information came from the supplied handoff and Betaflight configuration dump. The USB and recovery observations below were verified during this session.

| Item | Project information |
| --- | --- |
| Flight controller | DIATONE MAMBA F405 MK2 V2 |
| MCU | STM32F405RGT6, an Arm Cortex-M4 MCU |
| Main flash | 1 MiB, starting at `0x08000000` |
| Ordinary SRAM used by the project | 128 KiB, starting at `0x20000000` |
| Additional MCU RAM | 64 KiB CCM; unused by our initial bare-metal linker layout |
| USB connector | USB-C |
| Sensor, according to the handoff | ICM42688P variant |
| External oscillator, according to the handoff | 8 MHz HSE |
| Original application | Betaflight 4.4.3 |
| Original Betaflight target | `FURYF4OSD` |
| Manufacturer identifier in dump | `DIAT` |
| Build date recorded in original firmware | 3 September 2026 |

The planned IMU connections are:

| Signal | MCU pin |
| --- | --- |
| SPI1 clock, SCK | PA5 |
| SPI1 data from sensor, MISO | PA6 |
| SPI1 data to sensor, MOSI | PA7 |
| IMU chip select | PA4 |
| Gyro data-ready interrupt | PC4 |
| Sensor alignment recorded by Betaflight | `CW180` |

These IMU settings have not yet been exercised by our application. When we report board-aligned sensor axes later, the recorded orientation must be accounted for.

The generic STM32F405RG board definition identifies the MCU; it does not describe every MAMBA peripheral or connection. See the [PlatformIO board definition documentation](https://docs.platformio.org/en/latest/boards/ststm32/genericSTM32F405RG.html) and [ST's MCU datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf).

## 3. Files and directories

Paths in this table are relative to `/home/khishan/IMU`.

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Active MCU, framework, package versions, and build flags |
| `Makefile` | Runs the local PlatformIO build |
| `requirements-platformio.txt` | Pins PlatformIO Core to 6.2.0 |
| `src/main.cpp` | USB heartbeat application |
| `src/mamba_clock.c` | Board-specific clock configuration |
| `.tools/platformio/penv/` | Isolated Python environment containing PlatformIO Core |
| `.tools/platformio/packages/` | PlatformIO's downloaded compilers and frameworks |
| `.tools/dfu-util/` | Locally extracted Ubuntu `dfu-util` package |
| `.pio/build/mamba_usb/firmware.elf` | Linked firmware with addresses and symbols |
| `.pio/build/mamba_usb/firmware.bin` | Raw application bytes used for flashing |
| `bf-backup.txt` | Original Betaflight settings dump |
| `backups/betaflight-original-flash.bin` | Original FC main-flash backup |
| `backups/SHA256SUMS` | Recorded checksum of the original flash backup |
| `scripts/backup-fc.sh` | Read-only main-flash backup script |
| `standalone/` | Earlier bare-metal GCC build check |
| `arduino/` | Earlier Arduino CLI USB example and build configuration |

Names starting with a dot are normally hidden by file browsers. The tool and build directories are excluded from Git by `.gitignore`. Preserve the original backup separately from generated build files; a clean build directory can be recreated, but the original installed image is valuable historical data.

An **ELF** contains executable sections, symbols, and their memory addresses. A **BIN** is a raw sequence of bytes, so the flashing tool needs its destination address. An Intel **HEX** file stores bytes together with address records. Our active workflow flashes the BIN at `0x08000000`.

## 4. First milestone: prove the compiler can build for the MCU

We checked the compiler using:

```bash
arm-none-eabi-gcc --version
```

`--version` reports the installed compiler version without compiling anything. The system compiler reported **GCC 14.2.1**.

`arm-none-eabi-gcc` is a cross-compiler: it runs on Ubuntu but produces code for an Arm embedded target. Plain `gcc` normally targets the host computer. Installing a compiler does not install firmware on the FC.

We created and built the `standalone/` example:

```bash
cd /home/khishan/IMU/standalone
make
```

`cd` changes the current directory. `make` reads that directory's `Makefile` and runs the required compile/link steps. This Makefile has no flashing target.

The example's `main()` executes an infinite loop containing `nop`, an instruction that performs no useful operation. It has no USB or sensor support. Its purpose was to confirm the compiler and linker setup before introducing a framework.

The files work together as follows:

| File | Responsibility |
| --- | --- |
| `main.c` | Application entry point and infinite loop |
| `startup.S` | Core vector table, reset handler, C memory initialization |
| `stm32f405.ld` | Flash/RAM layout and placement of program sections |
| `Makefile` | Compile, link, and report application size |

At reset, the MCU obtains the initial stack pointer and reset-handler address from the vector table. Our startup code copies initialized data into RAM, clears zero-initialized variables, and calls `main()`. Interrupts remain disabled in this baseline; it only includes the core exception vectors.

The important compiler/linker options in that Makefile are:

| Option | Explanation |
| --- | --- |
| `-mcpu=cortex-m4` | Generate instructions for the Cortex-M4 |
| `-mthumb` | Generate the Thumb instruction encoding used by this MCU |
| `-mfloat-abi=soft` | Use the software floating-point calling convention in this simple baseline |
| `-Os` | Optimize for program size |
| `-g3` | Include debugging information in the ELF |
| `-ffreestanding` | Compile for an environment without a normal operating-system runtime |
| `-fno-builtin` | Avoid assuming standard library functions are supplied |
| `-ffunction-sections`, `-fdata-sections` | Put functions/data into separate sections |
| `-Wall -Wextra -Werror` | Enable compiler warnings and treat them as errors |
| `-nostdlib` | Link without the standard startup/runtime libraries |
| `-T stm32f405.ld` | Use our linker script |
| `-Wl,--gc-sections` | Ask the linker to discard unused sections |
| `-Wl,-Map=build/minimal.map` | Write a map showing where linked sections are placed |
| `-Wa,--noexecstack` | Mark object files as not requiring an executable stack |

The `-Wa,` and `-Wl,` prefixes forward options to the assembler and linker. We added the executable-stack metadata after the first build reported a linker warning. The final build completed without that warning.

We also used:

```bash
make -B
arm-none-eabi-readelf -h build/minimal.elf
```

`make -B` forces rebuilding. `readelf -h` inspects the ELF header, including its target architecture and entry address. The baseline contained **140 bytes of code/read-only data**. Successful compilation alone did not demonstrate hardware operation, and this baseline was not flashed.

## 5. Why we added STM32duino, then moved to PlatformIO

The intended [finani/ICM42688 library](https://github.com/finani/ICM42688) is Arduino-based and provides SPI and FIFO APIs. It is source code that must be compiled into an application. STM32duino supplies the Arduino-compatible environment on STM32 hardware.

There are three distinct layers:

| Layer | Role |
| --- | --- |
| GCC toolchain | Compiles and links C/C++ into MCU instructions |
| STM32duino framework | Supplies startup/runtime support, Arduino APIs, and peripheral support |
| Arduino CLI or PlatformIO | Selects packages/configuration and orchestrates builds |

We initially used Arduino CLI to establish that STM32duino could compile a USB example for the MCU. We subsequently selected PlatformIO because it keeps the maintained project configuration and dependencies together. PlatformIO continues using STM32duino for this project. Its STM32 documentation describes the official Arduino core and USB CDC flags: [STM32 platform configuration](https://docs.platformio.org/en/latest/platforms/ststm32.html).

### Historical Arduino CLI setup

We downloaded a Linux Arduino CLI archive and extracted its executable locally:

```bash
curl -fL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz \
  -o /tmp/imu-arduino-cli.tar.gz
mkdir -p /home/khishan/IMU/arduino/.tools/bin
tar -xzf /tmp/imu-arduino-cli.tar.gz \
  -C /home/khishan/IMU/arduino/.tools/bin arduino-cli
```

`curl` downloads a file. `-f` reports HTTP failure as an error, `-L` follows redirects, and `-o` names the output. `mkdir -p` creates directories, including missing parents. `tar -xzf` extracts a gzip-compressed archive; `-C` selects the destination. This archive's executable reported **1.5.2-rc.1**, a release candidate. It is historical setup, not a required dependency of the active PlatformIO project.

From the workspace we ran:

```bash
arduino/.tools/bin/arduino-cli --config-file arduino/arduino-cli.yaml core update-index
arduino/.tools/bin/arduino-cli --config-file arduino/arduino-cli.yaml core install STMicroelectronics:stm32
```

`--config-file` selects our project-local settings. `core update-index` retrieves package catalogs; `core install` downloads the STM32 framework and its required tools. The configuration supplied STM32duino's additional package-index URL and placed its data, downloads, and user directories under `arduino/.tools/`.

The board selection was:

```text
STMicroelectronics:stm32:GenF4:pnum=GENERIC_F405RGTX,usb=CDCgen
```

This fully qualified board name selects the generic F405RG target and USB CDC on `Serial`. Running `make` in `arduino/` compiled the sketch without uploading it. That build reported **19,860 bytes flash and 4,628 bytes RAM**. For current work, use the root PlatformIO project.

## 6. The active PlatformIO build

### Local installation

PlatformIO Core is pinned to **6.2.0** in `requirements-platformio.txt`.

The first installation attempt encountered Ubuntu's missing `ensurepip`/venv support. We initially bootstrapped pip inside a local environment. Later, when the local executable and build output were unavailable before flashing, we recreated the environment using the available Miniconda Python and rebuilt. The final successful setup commands were:

```bash
cd /home/khishan/IMU
/home/khishan/miniconda3/bin/python -m venv /home/khishan/IMU/.tools/platformio/penv
.tools/platformio/penv/bin/python -m pip install --no-cache-dir -r requirements-platformio.txt
```

`python -m venv` creates an isolated Python environment. Its packages live in the specified directory. `python -m pip` invokes that environment's package manager. `-r` reads package requirements from a file, and `--no-cache-dir` disables pip's download cache. Miniconda's Python was already available; installing Miniconda is not a requirement for the firmware itself.

On a different machine, use a Python installation with working venv support and create the same environment, or use PlatformIO's [official installation procedure](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html). The root Makefile expects our local executable path. Simply typing `pio` may fail if that executable is not on the shell's `PATH`.

### Project configuration

The active `platformio.ini` is:

```ini
[platformio]
default_envs = mamba_usb
core_dir = .tools/platformio

[env:mamba_usb]
platform = platformio/ststm32@20.0.0
board = genericSTM32F405RG
framework = arduino
platform_packages =
    platformio/framework-arduinoststm32@4.30000.0
build_flags =
    -D PIO_FRAMEWORK_ARDUINO_ENABLE_CDC
    -D HSE_VALUE=8000000U
monitor_speed = 115200
```

| Setting | Meaning |
| --- | --- |
| `default_envs` | Build `mamba_usb` by default |
| `core_dir` | Keep PlatformIO package/state storage in the workspace |
| `[env:mamba_usb]` | Name one build configuration |
| `platform ... @20.0.0` | Pin the STM32 platform release |
| `board` | Select STM32F405RGT6 MCU support and its memory layout |
| `framework = arduino` | Compile with STM32duino's Arduino-compatible framework |
| Framework package `4.30000.0` | Pin the PlatformIO package corresponding to upstream STM32duino 3.0.0 |
| `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` | Enable USB serial with generic `Serial` |
| `HSE_VALUE=8000000U` | Tell the core the external oscillator is 8,000,000 Hz |
| `monitor_speed` | Set the PlatformIO serial monitor's configured baud rate |

The platform and framework are pinned exactly; other tools follow this platform release's package constraints. The verified build used **GCC 12.3.1**, **CMSIS 6.3.0**, and **CMSIS DSP 1.16.2**. This GCC is supplied by PlatformIO and is separate from the system GCC 14.2.1 used for the first baseline.

### Build commands

```bash
cd /home/khishan/IMU
make
```

The root Makefile exports `PLATFORMIO_CORE_DIR` and invokes the local CLI. The equivalent direct command is:

```bash
PLATFORMIO_CORE_DIR="$PWD/.tools/platformio" \
  .tools/platformio/penv/bin/pio run --project-dir "$PWD" -e mamba_usb
```

`$PWD` expands to the current directory. The environment assignment applies to this invocation. `run` builds the firmware, `--project-dir` selects the project, and `-e` selects the environment. These commands do not flash the FC. The first build can download substantial dependencies; subsequent builds reuse them.

The verified PlatformIO application used **19,900 bytes flash** and **4,628 bytes RAM**. Its BIN file occupied **20,324 bytes**. Linker placement and padding can make the raw file length differ from the size report's allocated-section total.

## 7. What the heartbeat and clock code do

The heartbeat source is small enough to read in full:

```cpp
#include <Arduino.h>

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  if (Serial) {
    Serial.print("MAMBA STM32F405 USB heartbeat; uptime_ms=");
    Serial.println(millis());
  }
  delay(1000);
}
```

`setup()` runs once; `loop()` runs repeatedly. Our configuration makes `Serial` use USB CDC. `Serial.begin(115200)` supplies a conventional serial setting; USB transfers do not travel at the baud rate of a physical UART wire.

The `if (Serial)` condition checks whether the serial connection is ready. We do not block startup waiting for a terminal. `millis()` supplies elapsed milliseconds since startup, `println()` ends the line, and `delay(1000)` waits approximately one second. Actual observed increments were about 1,010 ms; we did not measure absolute timing accuracy.

`src/mamba_clock.c` supplies a strong `SystemClock_Config()` function, replacing the generic framework's weak default. It selects the handoff's external oscillator and calculates:

```text
8 MHz HSE / PLLM 4 = 2 MHz PLL input
2 MHz × PLLN 168 = 336 MHz PLL VCO
336 MHz / PLLP 2 = 168 MHz system clock
336 MHz / PLLQ 7 = 48 MHz USB clock
```

The code also configures the AHB/APB dividers, regulator scaling, and flash latency. `HSE_VALUE` must agree with the crystal configuration so the framework's frequency calculations are consistent. Defining the oscillator frequency alone does not configure the PLL; the C function performs that setup. The generic USB FS implementation uses PA11/PA12, with VBUS detection disabled by default in the core we inspected.

We verified that the ELF linked our strong clock function and USB serial code:

```bash
.tools/platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm \
  .pio/build/mamba_usb/firmware.elf
```

`nm` lists symbols; our inspection found `SystemClock_Config` as a text/code symbol and `SerialUSB`. We also inspected disassembly and checked that the BIN had plausible reset vectors and contained the heartbeat message. Hardware USB output subsequently demonstrated that the application could start and communicate. For the MCU clock architecture, see [ST's datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf).

## 8. The two backups and what each preserves

### Betaflight settings dump

Before the custom flash, Betaflight's CLI produced:

```text
dump all
```

This is entered at Betaflight's prompt, not at Ubuntu's shell prompt. It prints configuration commands, stored in `bf-backup.txt`. The output includes board identification, resources, settings, profiles, and a final `save` command. That trailing `save` belongs to the printed restore script.

The dump is useful for restoring settings after installing compatible Betaflight firmware. It does not contain the compiled Betaflight executable or the custom FIFO implementation's source code.

The handoff mentioned earlier files under `~/betaflight-imu`, including `betaflight_4.4.3_FURYF4OSD_FIFO.hex` and `.bin`. That directory and those images were not found in the locations searched. We therefore read the installed image from the FC itself.

### Main-flash binary backup

While the original firmware was still installed and the FC was in DFU, you ran:

```bash
cd /home/khishan/IMU
sudo ./scripts/backup-fc.sh
```

`sudo` gives the command the privileges needed to access the USB device. Ubuntu had returned `LIBUSB_ERROR_ACCESS` without it. The script's core operation was:

```bash
.tools/dfu-util/extracted/usr/bin/dfu-util \
  -d ,0483:df11 -a 0 -s 0x08000000:1048576 \
  -U backups/betaflight-original-flash.bin.partial
```

`-U` reads device data to the computer. `-s` specifies the start address and read length. The leading comma in `-d ,0483:df11` restricts the match to DFU-mode devices. `-a 0` selects the internal-flash alternate setting on this FC. These option meanings are documented in the [dfu-util manual](https://dfu-util.sourceforge.net/dfu-util.1.html).

The script uses a `.partial` file during transfer, requires exactly **1,048,576 bytes**, and then renames it to the final backup name. It refuses to overwrite an existing backup or partial file. It does not request erase, unprotect, or programming operations. Now that the FC runs the heartbeat, another read would capture the current flash, not recreate the original Betaflight image. Keep the original backup.

We checked:

| Check | Result |
| --- | --- |
| File length | 1,048,576 bytes |
| Initial stack pointer | `0x1000fff0`, within the MCU's CCM RAM range |
| Reset vector | `0x08079db9`, a plausible Thumb reset address in main flash |
| Identifying strings | `Betaflight`, `4.4.3`, `FURYF4OSD`, original build date |
| Erased/empty image | No |

Recorded SHA-256:

```text
11fdd6541ada5e25bcf3d524506223cfba135f26326563bc2476156d0dba5c7d
```

To check the saved file later:

```bash
cd /home/khishan/IMU/backups
sha256sum -c SHA256SUMS
```

`-c` checks files against recorded hashes. The verified result was `betaflight-original-flash.bin: OK`. A checksum establishes file integrity against that recorded value; it does not prove that restoration will succeed.

This backup captures the installed main-flash contents, including configuration stored there. It does not capture external blackbox storage, MCU option bytes, or the source repository. We did not independently verify the old FIFO command's implementation from the binary.

## 9. Getting dfu-util without installing it system-wide

The system initially had no `dfu-util` executable. We downloaded Ubuntu's package into the workspace and extracted it:

```bash
mkdir -p /home/khishan/IMU/.tools/dfu-util
cd /home/khishan/IMU/.tools/dfu-util
apt-get download dfu-util
cd /home/khishan/IMU
dpkg-deb -x .tools/dfu-util/dfu-util_0.11-3build1_amd64.deb \
  .tools/dfu-util/extracted
```

`apt-get download` downloads the package into the current directory; it does not install it. `dpkg-deb -x` extracts package files into a chosen directory. The package filename shown is the actual version used here; another Ubuntu release may download a different filename. The extracted executable uses the host's installed `libusb` library.

To list the target's DFU interfaces:

```bash
sudo .tools/dfu-util/extracted/usr/bin/dfu-util -d ,0483:df11 -l
```

`-l` lists interfaces; it does not program them. Selecting the STM32 ID avoids targeting unrelated USB devices. We used `sudo` after the unprivileged tool could see but could not open the FC. Do not remove read protection or use mass erase to work around a read/access error.

## 10. Serial mode, software DFU, and physical recovery

DFU means **Device Firmware Upgrade**. In this session the STM32 bootloader exposed the internal flash over USB, allowing the host to read or program it. Simply entering the bootloader does not replace application flash.

| State | Observed USB ID | Host interface | Meaning |
| --- | --- | --- | --- |
| Original Betaflight running | `0483:5740` | `/dev/ttyACM0` when present | Betaflight serial CLI/MSP interface |
| STM32 DFU bootloader | `0483:df11` | USB DFU, normal serial port disappears | Read/program internal flash |
| Our heartbeat running | `0483:5740` | `/dev/ttyACM0` | USB output from our application |

The serial USB ID alone does not distinguish Betaflight from our heartbeat. The actual application output does. USB bus/device numbers can change each time a device reconnects; they are not fixed identifiers.

### Software entry used before replacing Betaflight

We used `picocom`, a terminal serial program, rather than the graphical Betaflight Configurator:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
picocom -b 115200 /dev/ttyACM0
```

The `ls` command checks typical serial-device filenames. `*` is a wildcard, and `2>/dev/null` hides errors from patterns that have no matching device. `picocom -b` sets the configured baud rate; the final argument selects the device.

With Betaflight running, typing `#` entered its CLI. At that prompt we entered:

```text
bl
```

The FC rebooted into its bootloader. `picocom` lost the serial device and printed:

```text
FATAL: read zero bytes from port
term_exitfunc: reset failed for dev UNKNOWN: Input/output error
```

That disconnect was consistent with a reboot; DFU still had to be confirmed separately. The `bl` command belongs to Betaflight and is no longer available in our heartbeat application.

### Hardware entry that we verified

Initially the button was hard to identify. User photos showed a side-mounted switch with a small actuator projecting out from the board edge. Pressing the actuator inward while reconnecting USB successfully selected DFU.

The procedure verified on this physical board is:

1. Keep the flight battery disconnected and unplug USB.
2. Press the side button's actuator inward and keep it held.
3. Plug in the USB cable while holding it.
4. Hold for about two seconds, release, and allow USB enumeration to finish.
5. Check for the STM32 DFU device.

```bash
/usr/bin/lsusb -d 0483:df11
```

Our successful check returned:

```text
Bus 003 Device 010: ID 0483:df11 STMicroelectronics STM Device in DFU Mode
```

This procedure provides entry independently of the application. It is why we could proceed with the custom flash: a backup supplies bytes to restore, while a working hardware boot path supplies access to write them. It does not require us to solder BOOT0 connections or use ST-Link for the verified procedure. ST describes its system-memory bootloader in [AN2606](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf).

## 11. How we flashed the heartbeat

**This is a write operation. It replaces the application on the FC.** It was already performed successfully in this session. Running it again would reprogram the current build.

Before flashing we confirmed the original backup's checksum, rebuilt the firmware, checked its reset vectors and heartbeat string, inspected linked clock/USB symbols, and verified the target was in DFU.

The command you ran was:

```bash
cd /home/khishan/IMU
sudo .tools/dfu-util/extracted/usr/bin/dfu-util \
  -d ,0483:df11 \
  -a 0 \
  -s 0x08000000:leave \
  -D .pio/build/mamba_usb/firmware.bin
```

| Part | Meaning |
| --- | --- |
| `sudo` | Obtain USB access privileges |
| `-d ,0483:df11` | Match the STM32 in DFU mode |
| `-a 0` | Select internal flash on this FC |
| `-s 0x08000000:leave` | Write at main-flash start, then request exit from DFU |
| `-D ...firmware.bin` | Transfer the host's firmware file into the device |

The line-ending backslashes continue one shell command across several lines. Avoid adding characters or spaces after them. The tool's terminology is from the device's perspective: **`-U` reads device → computer; `-D` writes computer → device**. See the [dfu-util manual](https://dfu-util.sourceforge.net/dfu-util.1.html).

The command let the tool erase the flash sectors needed for the new image and write its **20,324 bytes**. It did not request a full-chip mass erase. Remaining old flash contents outside programmed sectors are not a usable substitute for the saved complete backup.

Your log showed:

```text
Downloading element to address = 0x08000000, size = 20324
Erase    done.
Download done.
File downloaded successfully
Submitting leave request...
Transitioning to dfuMANIFEST state
```

That established transfer completion. The subsequent USB heartbeat demonstrated that the flashed application ran. We did not perform a separate byte-for-byte post-flash readback comparison.

Keep the battery disconnected during these bench checks. Normal reconnection is performed without holding the side button; holding it selects DFU again.

## 12. Checking the heartbeat yourself

After flashing, our direct USB check found:

```text
Bus 003 Device 011: ID 0483:5740 STMicroelectronics Virtual COM Port
```

The serial device was `/dev/ttyACM0`. To check the available port and open it:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
picocom -b 115200 /dev/ttyACM0
```

Replace the port only if the listing shows a different one. Open one serial-monitor program at a time. We closed our temporary read connection after collecting three heartbeat lines, leaving the port available for you.

Messages should appear approximately once per second while the connection is ready. Exit picocom using **Ctrl+A, then Ctrl+X**. This is a sequence: press/release the first shortcut, then the second. See the [picocom project documentation](https://github.com/npat-efault/picocom).

In this application, typing `#`, `dump all`, or `bl` does not open a Betaflight CLI. Only the heartbeat output is implemented. Uptime starts over when the MCU restarts.

## 13. Interpreting the issues we encountered

### Slow setup and permission prompts

Early tools were running with restricted network access. A download failed with a name-resolution error, and commands needed explicit network permission. Compiler/framework downloads also took time. These delays concerned the host setup, not communication with the FC. Later the environment permitted network access directly.

### Missing local build tools/output

Before the flash, the local PlatformIO executable and `.pio` output were unavailable even though the source files remained. We recreated the Python environment and rebuilt successfully. The cause of their disappearance was not established. A generated build artifact must be checked before a command relies on it.

### Empty lsusb output in the user terminal

An empty filtered result from `lsusb -d 0483:df11` means that invocation did not list a matching DFU device. It does not, by itself, distinguish normal mode from an unplugged FC.

In this session even unfiltered `lsusb` appeared empty in the user terminal, while our direct tool calls showed USB hubs and peripherals. We requested an output capture:

```bash
/usr/bin/lsusb > /tmp/imu-lsusb-check.txt 2>&1
printf 'Exit code: %s\n' "$?"
cat /tmp/imu-lsusb-check.txt
```

`>` writes standard output to a file, replacing its previous contents. `2>&1` sends standard error to the same destination. `$?` is the previous command's exit status; zero normally indicates success. `printf` displays it, and `cat` displays the saved file. The reported exit code was zero with no listing. We did not establish the cause of this discrepancy. Hardware recovery was ultimately confirmed using a fresh direct USB listing after the user actuated the side button.

### DFU error status at the beginning of a transfer

The backup and flash logs initially reported `dfuERROR` with wording that the device's firmware was corrupt. The tool cleared the status and reached `dfuIDLE` with no error; the backup read and programming operation then succeeded. That initial message alone did not establish that the original Betaflight image was corrupt. Its cause was not independently diagnosed.

### Invalid DFU suffix warning

The heartbeat was supplied as a raw `.bin`, and dfu-util warned about an invalid/missing DFU suffix. In this observed run the warning did not prevent programming: the log reported a successful download and the application subsequently emitted the heartbeat. A successful transfer and working application were the relevant checks for this session.

### Serial permission denied

USB programmer access and serial-port access are different permissions. Our DFU tool needed `sudo`. The serial port's device permissions assigned it to the `dialout` group, and our serial read succeeded without `sudo`. If another account receives permission denied, inspect that account's groups and device permissions before changing them. No serial-permission change was needed here.

## 14. Restoring the original firmware if needed

**The following restore procedure has not been run or tested in this session. It is a future write operation that replaces the custom application with the saved original main flash.**

1. Close any serial monitor.
2. Use the verified side-button procedure to enter DFU.
3. Confirm the DFU device and verify the backup checksum.

```bash
/usr/bin/lsusb -d 0483:df11
cd /home/khishan/IMU/backups
sha256sum -c SHA256SUMS
```

4. If the correct device is present and the checksum passes, the saved binary's restore command is:

```bash
cd /home/khishan/IMU
sudo .tools/dfu-util/extracted/usr/bin/dfu-util \
  -d ,0483:df11 \
  -a 0 \
  -s 0x08000000:leave \
  -D backups/betaflight-original-flash.bin
```

This writes the complete saved main-flash image at its original base address. It may also restore settings stored in that image. After completion and normal USB reconnection, verify that Betaflight starts and inspect its version/settings before deciding whether to reapply the text dump. Avoid blindly applying the dump on top of already restored settings or to incompatible firmware.

The settings text contains CLI prompts and comments from the capture; use its actual restore commands through a compatible Betaflight CLI. Do not paste a Betaflight dump into Ubuntu's shell.

Neither the backup nor the heartbeat-flash command changes the external blackbox storage. The backup does not record MCU option bytes, and these instructions do not change them.

## 15. Current status and next milestone

| Capability | Status at end of this session |
| --- | --- |
| Bare-metal compiler/linker check | Passed |
| PlatformIO build | Passed |
| Original settings backup | Saved |
| Original main-flash backup | Saved; size/content/checksum checked |
| Physical-button DFU entry | Verified |
| Heartbeat flash | Completed |
| USB enumeration and heartbeat output | Verified |
| Backup restoration | Not tested |
| SPI sensor communication | Not implemented/tested |
| ICM42688 FIFO acquisition | Not implemented/tested |
| Absolute clock frequency measurement | Not performed |
| Flight control/motor operation | Not implemented |

The next milestone is to add the ICM42688 library to PlatformIO, configure the recorded SPI pins, compile a sensor example, and then test initialization and sensor output. Keep the working heartbeat as the USB baseline while bringing up SPI and the IMU.

## 16. Quick command reference

| Task | Command or procedure | Effect |
| --- | --- | --- |
| Build current application | `cd /home/khishan/IMU`, then `make` | Host build only |
| Show STM32 USB devices | `/usr/bin/lsusb -d 0483:` | Read-only listing |
| Check DFU specifically | `/usr/bin/lsusb -d 0483:df11` | Read-only listing |
| Check serial filenames | `ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null` | Read-only listing |
| Watch heartbeat | `picocom -b 115200 /dev/ttyACM0` | Opens serial connection |
| Exit picocom | Ctrl+A, then Ctrl+X | Closes serial connection |
| Check original backup integrity | Run `sha256sum -c SHA256SUMS` in `backups/` | Read-only check |
| Enter DFU now | Hold side actuator while connecting USB | Selects MCU bootloader |
| Program a BIN | Explicit dfu-util `-D` command from the flash/restore sections | Writes FC flash |

The original backup and the working USB baseline provide a practical starting point for the sensor work. Follow the current-status section when resuming; the historical Betaflight `bl` method no longer applies to the application installed now.
