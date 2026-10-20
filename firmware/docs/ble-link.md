# BLE link: what the firmware implements and how to test it

Hardware test procedure for whoever has the devkit: [HANDOVER-ble-hardware-test.md](HANDOVER-ble-hardware-test.md).

Status: **v0 builds clean for esp32s3 (ESP-IDF v5.5.1, 19 Oct 2026), not yet run on hardware**. Host tests pass (r03: 189 checks). Source of truth for
the contract is the app team's Notion task "Build the CAIPO mobile app: BLE
contract, plan and status", section "BLE contract" (agreed 15 Oct 2026).

Components involved:

| Component | Role |
|---|---|
| `components/ble_link` | NimBLE peripheral: GATT table, security, advertising, heartbeat, notify API |
| `components/r03_transfer` | R03 codec and the receiver/sender state machines, pure C, host-tested |
| `main/main.c` | v0 glue: event queue, R03 wiring, GET_STATUS / SET_TIME, touch demo transfer |

## GATT

```
Service   A1000000-5fa6-43f5-9827-201f62bd377d      advertised (128-bit UUID in the ADV PDU)
A1000001  notify            device -> app, R03 messages
A1000002  read, encrypted   codec ID = 0x15 (Opus 16 kHz mono, 20 ms)
A1000003  write, encrypted  app -> device, R03 messages (write with or without response)
A1000004  write, encrypted  app -> device commands
          notify            device -> app heartbeat, STATUS, CMD_RESULT
Battery   0x180F / 0x2A19   read + notify, placeholder 100 % until battery_adc exists
Name      "CAIPO" + 4 hex digits of the address, e.g. CAIPO3F2A, in the scan response
```

## Security

- LE Secure Connections, Just Works (the device has no display), bonding on,
  bonds in NVS (`CONFIG_BT_NIMBLE_NVS_PERSIST`). Legacy pairing is disabled.
- The device calls `ble_gap_security_initiate()` as soon as the phone connects.
  A bonded phone re-encrypts silently; a new phone gets the system pairing
  dialog. Reads of A1000002 and writes to A1000003 / A1000004 are refused by the
  stack until the link is encrypted, and the device sends no notification
  before `BLE_GAP_EVENT_ENC_CHANGE` reports encryption.
- If the phone forgot the bond but the device still has it, the device deletes
  its copy and lets the phone pair again (`BLE_GAP_EVENT_REPEAT_PAIRING`).
- Long press on the touch pad clears all bonds and disconnects.
- Just Works is encrypted but not authenticated against an active
  man-in-the-middle during the first pairing. That is the ceiling for a device
  with no display or keypad; it satisfies R07.1 (AES-128+ on the link).

## Connection parameters

Requested right after connect (the phone may change them):
interval 15 to 30 ms, latency 0, supervision timeout 6 s. These follow Apple's
accessory rules (min >= 15 ms, max >= min + 15 ms, max x 3 <= timeout). The
device logs what the phone actually granted on `BLE_GAP_EVENT_CONN_UPDATE`.
The app's own 3 s heartbeat rule remains the real disconnect detector.

## Heartbeat

Every 1000 ms (`CONFIG_CAIPO_BLE_HEARTBEAT_MS`) once connected, encrypted and
subscribed to A1000004: `[0x10][counter u8]`, counter wraps at 255.

## MTU and chunking

Preferred MTU 512 (`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU`). Chunk payload is
computed from the negotiated MTU as `min(502, MTU - 3 - 7)`
(`r03_chunk_payload_for_mtu`), so an iPhone at MTU 185 gets 175-byte payloads.
The payload size is fixed when a transfer starts; the MTU cannot change within
a connection.

## R03 behaviour on the device

Both directions share A1000001 (device speaks) and A1000003 (app speaks); the
roles are told apart by opcode and transfer id.

- **App -> device.** REQUEST gets READY; METADATA is checked (type 1..4, size
  within the device buffer, non-zero); CHUNKs must arrive in order. A gap
  returns `TRANSFER_ERROR SEQUENCE_GAP` with the seq the device needs, and the
  app resends from there. Duplicates are ignored. The last chunk triggers the
  CRC-32 check, then ACK. 5 s of silence returns `TIMEOUT` and frees the slot.
  A second REQUEST with a different id while one is in flight gets `BUSY`.
- **Device -> app.** The device sends REQUEST and waits for READY, then
  METADATA and CHUNKs as fast as the stack accepts them (back-pressure on
  `BLE_HS_ENOMEM`), then waits for ACK. `SEQUENCE_GAP` resumes from the failed
  seq, `CHECKSUM_MISMATCH` restarts from REQUEST, three resends then give up.
- **Device limits.** Inbound buffer is 64 KB in v0 (`RX_CAPACITY` in main.c);
  anything bigger gets `TOO_LARGE` even though the protocol allows 32 MB.
  Device-chosen transfer ids start at 0x8000 to avoid colliding with the app's.
  Both points are proposals to the app team, not yet in the contract.

## Control opcodes implemented

| Opcode | Direction | Behaviour |
|---|---|---|
| 0x10 heartbeat | device -> app | every second |
| 0x20 GET_STATUS | app -> device | answers 0x21 STATUS `[STANDBY][100][flags][0 0]` |
| 0x26 SET_TIME | app -> device | sets the system clock, answers 0x2F CMD_RESULT OK |
| any other 0x2x | app -> device | 0x2F CMD_RESULT UNSUPPORTED (2) |

## Testing without the app (nRF Connect or LightBlue)

Wokwi cannot run this part: it has no Bluetooth. The Wokwi build (README) compiles
the link out and the tests below need a real XIAO ESP32-S3.

1. Flash and open the monitor: `idf.py -p COMx flash monitor`. Expect
   `advertising as "CAIPOxxxx"`.
2. Scan for the name; connect. Expect the system pairing prompt on the phone
   and `link encrypted=1 ... bonded=1` in the log.
3. Enable notifications on A1000004: a two-byte notification arrives every
   second, second byte counting up.
4. Read A1000002: one byte, `0x15`.
5. Write `20` to A1000004: a 6-byte `21 00 64 00 00 00` notification comes back.
6. Write `01 01 00` to A1000003 (REQUEST id 1), enable notifications on
   A1000001 first: `02 01 00` (READY) comes back. Then write
   `03 01 00 05 00 00 00 03 xx xx xx xx 01 00` (METADATA: 5 bytes, type OTHER,
   CRC, 1 chunk) and `04 01 00 00 00 05 00 68 65 6C 6C 6F` (CHUNK "hello").
   CRC-32 of "hello" is `3610A686`, so the CRC bytes are `86 A6 10 36`.
   Expect `05 01 00` (ACK) and `transfer 1 complete: 5 bytes` in the log.
7. Tap the touch pad: the device sends a 3000-byte test transfer starting with
   `01 01 80` (REQUEST id 0x8001). Without an app answering READY, it times
   out after 5 s, which is the expected behaviour.

With the real app (`caipo-live.apk`): pair, watch the heartbeat indicator, then
tap the pad and check the transfer appears on the phone side.

## Not in v0 (v1 and later)

- Opus encode/decode, microphone capture into a recording, speaker playback of
  the reply. Antoine's `mic_inmp441` / `spk_max98357` are I2S loopback tests.
- Storage partition and the queue of recordings while the phone is away
  (needs the flash size confirmed: XIAO ESP32-S3 has 8 MB, `partitions.csv`
  still assumes 4 MB).
- PROPOSED commands that need the Pi (capture, video, sync, stream), WiFi
  provisioning to NVS and UART, battery from `battery_adc`, LED status
  patterns.

## Host tests

`test/host/test_r03.c` covers CRC-32, MTU to payload, codec round trips and
rejects, receiver happy path, sequence gap recovery, every error code,
timeout, busy, and a sender/receiver loopback with resend and failure paths.
Build with `test/host/CMakeLists.txt` or any C11 compiler (command in the file
header).
