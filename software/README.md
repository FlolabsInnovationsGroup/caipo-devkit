# Caipo Pi software (Raspberry Pi)

Python package (`caipo_pi`) for the Caipo DEV KIT's Raspberry Pi. The Pi side owns the camera, audio capture/processing, uploads, and AI inference, and talks to the ESP32-S3 over the UART protocol.

> **Status:** directory scaffold only — no implementation yet.

## Prerequisites

- Python pinned via `.python-version` (e.g. 3.11)
- [`uv`](https://github.com/astral-sh/uv) (or Poetry) for dependency management — commit `uv.lock`
- A provisioned Pi with the PL011 UART enabled (see `deploy/boot-config.txt`)

## Install, run

All commands run from this directory (`software/`):

```sh
uv sync                 # install pinned deps from uv.lock
uv run python -m caipo_pi   # entry point (src/caipo_pi/__main__.py)
uv run pytest           # test suite, no hardware required
```

Configuration is a `pydantic-settings` model (`config.py`): validated YAML + env overrides. Copy `deploy/config.example.yaml` as a starting point; the provisioning flow copies it to `/etc` on the device.

## Deploy

`deploy/provision.sh` takes a fresh Pi to a running unit in one command. It wires up:

- `caipo.service` — systemd unit (`Restart=on-failure`, `WatchdogSec`, `ReadWritePaths`)
- `boot-config.txt` — disables Bluetooth, fixes `core_freq`, enables the PL011 UART (the fix for the UART issue)
- `overlayfs.md` — how to make root read-only so power gating can't corrupt the SD card
- `udev/99-caipo.rules` — stable device names for the camera and serial port

## Architecture

Same layering discipline as the firmware — drivers at the bottom, orchestration on top.

```
src/caipo_pi/
  app.py              Orchestrator + state, mirrors the ESP state machine
  config.py            pydantic-settings model: validated YAML + env overrides
  logging_config.py    structlog/logging setup, journald-friendly

  comm/
    uart.py            pyserial-asyncio transport on ttyAMA0
    protocol.py         GENERATED from /protocol — same contract as the ESP side
    dispatcher.py        Maps a parsed command to a handler, sends ACK/COMPLETE/ERROR

  hardware/
    camera.py           picamera2 wrapper, 1080p30 H.264
    audio.py            Optional Pi-side audio capture/processing

  services/
    recorder.py         Drives a capture session start to finish
    storage.py          Naming convention, cleanup rules, free-space management
    uploader.py         WiFi on-demand, S3/Drive/custom, retry queue, .uploaded flag
    health.py           Heartbeat sender + systemd watchdog ping

  ai/
    inference.py         Model inference stub (TFLite/ONNX later)
```

Design rules:

- `comm/protocol.py` is regenerated from `/protocol/caipo.protocol.yaml`; edit the YAML, not the Python.
- `models/` holds AI models at runtime only — never committed to git.

### Note on the camera path

Capture goes through `picamera2` and its `H264Encoder` rather than shelling out to a raw `ffmpeg`/`v4l2` pipeline. On the command line, current Pi OS ships `rpicam-vid` / `rpicam-hello` / `rpicam-still` — the older `libcamera-*` commands no longer exist. `ffmpeg` is still useful for muxing and transcoding, just not as the capture front end.

## Tests

`tests/` needs no hardware — `conftest.py` wires in fakes (`fakes/fake_serial.py`, `fakes/fake_camera.py`) for the serial port and camera. `test_protocol.py` runs the **same** golden vectors from `/protocol/vectors/` that the ESP codec is tested against, so both sides are checked for drift the same way.

## Code style

`ruff` + `black` + `mypy`, configured in `pyproject.toml` and enforced by pre-commit.
