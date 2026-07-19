# Caipo firmware (ESP32-S3)

ESP-IDF firmware for the Caipo DEV KIT's ESP32-S3. The ESP side owns power management (including the Pi's enable pin), user input, the status LED, audio I/O, and the UART link to the Raspberry Pi.

## Prerequisites

- **ESP-IDF v5.5.x** (project developed against v5.5.4)
- Target: `esp32s3`
- On Windows, run commands from an ESP-IDF terminal (or `export.bat` first); on Linux/macOS, `source export.sh`.

## Build, flash, monitor

All commands run from this directory (`firmware/`):

```sh
idf.py set-target esp32s3     # once, before the first build
idf.py build
idf.py -p <PORT> flash monitor   # Ctrl+] exits the monitor
idf.py menuconfig             # tunables live in Kconfig, not in source
idf.py fullclean              # wipe the build directory
```

`scripts/flash.sh` is a thin wrapper for `idf.py -p $PORT flash monitor`.

Configuration is layered: `sdkconfig.defaults` (committed base config) + `sdkconfig.defaults.esp32s3` (S3-specific overrides). The generated live `sdkconfig` is machine-specific and gitignored — persistent settings go in the defaults files.

## Debugging

The ESP32-S3's built-in USB-JTAG is used with OpenOCD (`board/esp32s3-builtin.cfg`). VS Code users get this preconfigured via the ESP-IDF extension; clangd IntelliSense needs a successful build first (it reads `build/compile_commands.json`).

## Architecture

ESP-IDF component model with strict layering — drivers at the bottom, managers in the middle, the app on top. **Lower layers never call upward.**

```
main/                 Boot only: init board, create FreeRTOS tasks, then idle.
                      No business logic lives here.
components/
  caipo_bsp/          Board support. The ONLY place a pin number is defined
                      (caipo_pins.h, kept in sync with docs/PINMAP.md).
  caipo_protocol/     GENERATED from /protocol — do not hand-edit.
                      Host-compilable codec, no IDF dependency.
  uart_link/          Transport over caipo_protocol: RX/TX tasks, framing,
                      heartbeat, timeout, retry.
  power_mgr/          Power states, Pi enable pin (S13V30F5), low-battery
                      logic, sleep entry/exit. Thresholds via Kconfig.
  led_ws2812/         RGB status LED (RMT driver + documented colour map).
  touch_ttp223/       Touch input: debounce, tap / double-tap / long-press.
  battery_adc/        Calibrated battery voltage (ADC1 + divider math).
  mic_inmp441/        I2S microphone capture (DMA, WAV helper).
  spk_max98357/       I2S speaker playback (tones and clips).
  caipo_app/          The brain: state machine (STANDBY / WAKE / POWERING_UP /
                      PI_BOOTING / ACTIVE / SHUTDOWN / ...) and workflows
                      (quick video, audio only, scheduled wake).
```

Design rules:

- Each concern runs as its own FreeRTOS task talking over queues — event-driven, not one giant superloop.
- Tunables (battery thresholds, baud, heartbeat interval, timeouts) go in **Kconfig** and are set via `menuconfig`, never as magic numbers in a header.
- `caipo_protocol` is regenerated from `/protocol/caipo.protocol.yaml`; edit the YAML, not the C.

## Tests

- **On-target Unity tests** live inside components (`components/<name>/test/`), e.g. framing/retry tests for `uart_link`, transition tests for the state machine.
- **Host tests** in `test/host/` build on the dev machine with plain CMake (no ESP-IDF) and run the protocol codec against the shared golden vectors in `/protocol/vectors/` — the same vectors the Pi side tests against.

## Code style

C formatting is enforced by `.clang-format` via pre-commit.
