# Component Implementation Steps

Working checklists for bringing up each component, in rough dependency order.
Check items off as they land; add discoveries (pin numbers, gotchas) inline so
this stays the single source of truth for bring-up.

Conventions that apply to every component:

- Pin assignments live in `components/caipo_bsp/include/caipo_pins.h`, never in drivers.
- Each driver exposes a small board-level API in `include/<name>.h`; the rest of
  the app must not see chip/peripheral details (RMT, I2S, ADC handles, ...).
- Application behavior (patterns, workflows, state) goes in `caipo_app`, not in drivers.
- When a component starts including another component's headers, add it to
  `REQUIRES`/`PRIV_REQUIRES` in its `CMakeLists.txt`.

---

## led_ws2812 — addressable status LED

Status: **not started**

- [ ] **Hardware facts** — record in `caipo_pins.h`:
  - [ ] `CAIPO_PIN_LED_DATA` — GPIO driving the WS2812 data line (TBD, from schematic)
  - [ ] `CAIPO_LED_COUNT` — number of LEDs on the chain (TBD)
- [ ] **Dependency** — add `espressif/led_strip: "^3"` to
      `components/led_ws2812/idf_component.yml` (RMT-backed managed component;
      fetched automatically on build). No hand-written RMT encoder.
- [ ] **Public API** in `include/led_ws2812.h`:
  - [ ] `led_init(void)`
  - [ ] `led_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b)`
  - [ ] `led_fill(uint8_t r, uint8_t g, uint8_t b)`
  - [ ] `led_clear(void)`
  - [ ] `led_set_brightness(uint8_t pct)` — software scaling applied before writes
- [ ] **Implementation** in `led_ws2812.c` — wrap `led_strip_new_rmt_device()`
      using the pin/count from `caipo_pins.h`; refresh after writes.
- [ ] **Build wiring** — `CMakeLists.txt`: add `REQUIRES caipo_bsp`.
- [ ] **Smoke test** — color cycle from `main.c`; `idf.py build flash monitor`
      and confirm the LED changes color on the board.
- [ ] **Later (caipo_app, not here)** — status patterns: charging, listening,
      error, etc., built on this API.

Notes / gotchas:
- Drive the data line low during board init so the LED doesn't latch random
  colors at boot (belongs in `caipo_bsp/board_init.c`).
- WS2812 wants ~5V data; if the board level-shifts, nothing changes in firmware,
  but if it drives at 3.3V and the LED misbehaves, suspect signal integrity
  before blaming timing.

---

## caipo_bsp — board support (pin map, board init)

Status: **not started** — first real content arrives with led_ws2812 (pin map entries).

Steps: TBD

## touch_ttp223 — touch sensor

Status: **implemented** — pending on-hardware smoke test and pin verification.
Files: `touch_ttp223.c` (driver), `touch_gesture.c/.h` (pure classifier),
`Kconfig` (CAIPO_TOUCH_* options), host test in `test/host/test_touch_gesture.c`
(needs a host C compiler — none installed on this machine yet).

The TTP223 is an external capacitive touch module with a plain digital output —
this is a GPIO-input driver, NOT the ESP32-S3's native touch-pad peripheral and
NOT I²C (the doc's §7.2 "I²C Protocol" heading and the `i2c_scan` example
showing 0x2A are inconsistencies; the pin map, register table, and block diagram
all say plain GPIO read, active high).

Hardware facts (from doc, Appendix A):
- Board is a **Seeed XIAO ESP32-S3**; touch is "GPIO 0 / D0" → TTP223 OUT
- Active HIGH = touched, momentary mode
- Doc-recommended timing: poll every 50 ms, 20 ms software debounce
- ⚠ **Verify the real GPIO number on hardware**: on the actual XIAO ESP32-S3,
  silkscreen D0 is chip GPIO1 (D0..D10 = GPIO 1,2,3,4,5,6,43,44,7,8,9 — the
  doc's "GPIO" column looks like the Dx index, not the chip GPIO). Also chip
  GPIO0 is a strapping pin (boot mode) — an active-high touch on real GPIO0
  at reset would be a design smell. Confirm with a multimeter/test firmware.

### Implementation plan (current scope: polled driver + gesture events)

One pass: a polled, debounced GPIO driver whose pin state feeds a
host-testable gesture classifier emitting events to a registered callback.

- [x] **Pins** — `caipo_pins.h`: `CAIPO_PIN_TOUCH` = GPIO1 (XIAO D0; verify
      on hardware) — first real content in `caipo_bsp`; rest of the pin map
      recorded as TODO comments with the doc's Dx→GPIO translation
- [x] **Public API** in `include/touch_ttp223.h`:
  - [x] `touch_event_t` enum: PRESS, RELEASE, TAP, DOUBLE_TAP, LONG_PRESS
  - [x] `touch_event_cb_t` callback with `void *ctx`
  - [x] `esp_err_t touch_init(void)`
  - [x] `esp_err_t touch_register_callback(touch_event_cb_t cb, void *ctx)`
        (single subscriber; caipo_app is the only consumer)
  - [x] `bool touch_is_pressed(void)` — instantaneous debounced state
- [x] **Gesture classifier as pure logic** (`touch_gesture.c/.h`, no ESP-IDF
      includes; `touch_gesture_feed(sm, pressed, now_ms, out[])` returns 0–2
      events; also contains the pure `touch_debounce_*` helper):
  - [x] Classification rules (doc §8.1 semantics):
        - PRESS / RELEASE: emitted on debounced edges
        - TAP: released before long-press threshold, and no second press
          within the double-tap window
        - DOUBLE_TAP: second press starts within the double-tap window
        - LONG_PRESS: still held at threshold (2000 ms per doc) — fires
          **while held**, once; release afterwards emits only RELEASE;
          a hold after DOUBLE_TAP fires nothing extra
  - [x] Host-side unit test written (`test/host/test_touch_gesture.c`; plain
        CMake in `test/host/CMakeLists.txt`): clean tap, long press, double
        tap, two slow taps, late-press window resolution, debounce
  - [ ] Host test **executed** — blocked: no host C compiler on this machine
        (esp-clang has no x86 backend, no MSVC/gcc). Install e.g. VS Build
        Tools or MinGW, then:
        `cmake -S test/host -B test/host/build && cmake --build test/host/build && ctest --test-dir test/host/build`
- [x] **Driver** in `touch_ttp223.c`:
  - [x] `gpio_config()` plain input (TTP223 output is push-pull, no pull),
        no interrupt
  - [x] FreeRTOS task: poll → debounce (≥2 consecutive polls, derived from
        `CAIPO_TOUCH_DEBOUNCE_MS`) → `touch_gesture_feed()` → callback from
        task context; timestamps from `esp_timer_get_time()/1000`
  - [x] Events suppressed for `CAIPO_TOUCH_STARTUP_BLANK_MS` (500) after boot
        (TTP223 self-calibration window)
- [x] **Timing constants via Kconfig** (`components/touch_ttp223/Kconfig`):
      `CAIPO_TOUCH_POLL_MS` (50), `CAIPO_TOUCH_DEBOUNCE_MS` (20),
      `CAIPO_TOUCH_LONGPRESS_MS` (2000), `CAIPO_TOUCH_DOUBLETAP_WINDOW_MS`
      (300), `CAIPO_TOUCH_STARTUP_BLANK_MS` (500)
- [x] **Build wiring** — `CMakeLists.txt`: SRCS both `.c` files,
      `REQUIRES caipo_bsp`, `PRIV_REQUIRES driver esp_timer`; `caipo_bsp`
      now `REQUIRES driver` (its public header includes `driver/gpio.h`)
- [x] **Smoke test code** — `main.c` registers a callback logging each event
      name
- [ ] **Smoke test on hardware** — `idf.py flash monitor`: verify tap,
      double-tap, 2 s hold; this also settles the GPIO1-vs-GPIO0 question.
      If `led_ws2812` is up later, map events to colors for no-PC verification.
- [ ] **Note the TAP latency trade-off**: a TAP can only be confirmed after
      the double-tap window expires (~300 ms after release). If caipo_app
      later needs snappier response, it can act on PRESS and refine.

### Later (not now)

- [ ] Switch from polling to interrupt-driven (both edges → queue → task)
      if power profiling shows polling matters in standby
- [ ] Wake-from-standby / light-sleep integration with `power_mgr`
      (doc: touch is a STANDBY exit condition)
- [ ] Gesture → action mapping (tap = wake+video, double-tap = audio record,
      long-press = shutdown) lives in `caipo_app`'s state machine, not here

## battery_adc — battery voltage

Status: **not started**

Steps: TBD

## uart_link — UART transport

Status: **not started**

Steps: TBD

## mic_inmp441 — I2S microphone

Status: **not started**

Steps: TBD

## spk_max98357 — I2S speaker amp

Status: **not started**

Steps: TBD

## caipo_protocol — message protocol (host-testable)

Status: **not started**

Steps: TBD

## power_mgr — power management

Status: **not started**

Steps: TBD

## caipo_app — state machine & workflows

Status: **not started** — depends on drivers and protocol.

Steps: TBD
