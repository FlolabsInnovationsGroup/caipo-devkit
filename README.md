# Caipo DEV KIT

Monorepo for the **Caipo DEV KIT**  — a dual-MCU device built from an **ESP32-S3** (always-on power management, sensors, user input) and a **Raspberry Pi** (camera, audio, AI, connectivity), joined by a UART protocol.

Everything ships together in one repo on purpose: the firmware, the Pi software, and the UART protocol that joins them live side by side so a protocol change updates both sides in one atomic commit. The hardware design lives here too, so a pin change and the firmware that uses that pin move together.

## Repository layout

| Directory | Contents | Status |
|---|---|---|
| [`firmware/`](firmware/) | ESP32-S3 firmware (ESP-IDF, C) — see its [README](firmware/README.md) | scaffolded |
| `software/` | Raspberry Pi software (Python package `caipo_pi`) | planned |
| `protocol/` | Single source of truth for the UART contract (`caipo.protocol.yaml` + codegen + golden test vectors) | planned |
| `hardware/` | KiCad schematic, PCB, BOM, datasheets | planned |
| `docs/` | Architecture docs, runbooks, ADRs, and the canonical pin map (`PINMAP.md`) | planned |
| `tools/` | Cross-side scripts: protocol codegen, flash-all, release | planned |
| `.github/` | CI workflows (path-filtered per side), CODEOWNERS, templates | planned |

## Key conventions

- **The protocol is generated, never hand-edited.** `protocol/caipo.protocol.yaml` defines every message once; codegen emits the C codec into `firmware/components/caipo_protocol/` and the Python codec into `software/src/caipo_pi/comm/protocol.py`. Both sides test against the same golden vectors in `protocol/vectors/`, and CI fails if either committed copy drifts.
- **Exactly one place defines a pin.** `docs/PINMAP.md` is the human source of truth; `firmware/components/caipo_bsp/include/caipo_pins.h` must match it.
- **One device version.** A single semver in `VERSION` covers firmware + software + protocol; releases are one tag.
- **Trunk-based development** — short-lived feature branches, protected `main`.
- **Secrets never in git.** WiFi/cloud keys go in encrypted NVS on the ESP and an env file or secrets store on the Pi, provisioned at flash time.

## Quick start

### Firmware (ESP32-S3)

Requires ESP-IDF v5.5.x. From `firmware/`:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

See [`firmware/README.md`](firmware/README.md) for details.

### Pi software / protocol / hardware

Not yet in the repo — see the layout table above for where each will land.

## License


