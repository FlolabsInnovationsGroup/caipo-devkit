# protocol/ — Caipo UART contract

Single source of truth for the message layer of the ESP32-S3 <-> Raspberry Pi UART link.

`caipo.protocol.yaml` defines every message once (payload fields only — msg_type + seq
header, then fields in declared order, little-endian, no padding). Codegen renders that
definition into two codecs that must stay byte-for-byte identical:

- `software/src/caipo_pi/comm/protocol.py` — Python codec
- `firmware/components/caipo_protocol/{include/caipo_protocol.h,caipo_protocol.c}` — C codec, host-compilable, no ESP-IDF dependency

**Never hand-edit the generated files.** Edit `caipo.protocol.yaml`, then regenerate:

```sh
pip install pyyaml   # one-time; codegen's only dependency
python3 protocol/codegen/generate.py
```

## What this layer is not

`caipo_protocol` (and its Python counterpart) is a pure message codec: it turns a typed
struct/dataclass into bytes and back. It does not know about:

- **Framing** — start/stop bytes, escaping, checksums. That's `uart_link` (ESP side) and
  `comm/uart.py` (Pi side), layered on top of this codec.
- **Retries, timeouts, heartbeat scheduling** — also `uart_link` / `comm/uart.py`.

## Layout

```
caipo.protocol.yaml   The schema. Hand-edited.
codegen/
  schema.py            Parses + validates the YAML into dataclasses (schema.Protocol)
  render_python.py      Protocol -> comm/protocol.py source text
  render_c.py           Protocol -> caipo_protocol.h / caipo_protocol.c source text
  generate.py           CLI entry point wiring the above together
vectors/
  <message_name>.json   Golden encode/decode fixtures, one file per message. Both
                         software/tests/test_protocol.py and
                         firmware/test/host/test_protocol_host.c are meant to run
                         against these so the two codecs are checked against each
                         other, not just against themselves.
```

## Adding a message or field

1. Edit `caipo.protocol.yaml` (new message: pick an unused `id` in 0..255; new field:
   append rather than reorder, since field order is the wire order).
2. Run `python3 protocol/codegen/generate.py`.
3. Add or extend the matching fixture(s) under `vectors/`.
4. Regenerate/update both sides' tests against the new vectors.

## Field types

`u8 i8 u16 i16 u32 i32 f32 bool`, `enum:<Name>` (backed by `u8`, `<Name>` declared under
top-level `enums:`), and `string` (1-byte length prefix, UTF-8, `max_length` in bytes —
defaults to 64, settable per-field, must fit in `u8` i.e. 1..255).
