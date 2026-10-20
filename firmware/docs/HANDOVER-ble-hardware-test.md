# Handover: testing BLE v0 on the real XIAO ESP32-S3

Written 2026-10-20. The BLE link was developed and built on a machine with no
devkit and no Bluetooth simulator (Wokwi has none), so **this firmware has
never run on a board**. This page is everything you need to flash it, pair a
phone, and tell us what happened. Expect to spend 30 to 60 minutes.

Design and protocol details live in [ble-link.md](ble-link.md). This page is
only the test procedure.

## What you need

- XIAO ESP32-S3 devkit on USB. The touch pad (TTP223) wired as in the existing
  firmware; nothing else is required for the BLE tests.
- ESP-IDF v5.5.x installed. Run from an ESP-IDF terminal (`export.bat` /
  `source export.sh` done).
- A phone with **nRF Connect** (Android or iOS) or **LightBlue** (iOS).
- Optional: the Flutter app build from Maria's team (`caipo-live.apk`).

## Get the code

```sh
git fetch
git checkout Sosina-Raspberry_Pi
cd firmware
```

Check that `components/ble_link/` and `components/r03_transfer/` exist. If
they do not, the branch has not been pushed yet; ask Sosina.

## Build and flash

```sh
idf.py set-target esp32s3          # once; creates sdkconfig
idf.py -p <PORT> flash monitor     # Ctrl+] exits the monitor
```

If `set-target` fails, do not continue: a missing `sdkconfig` makes the next
build silently target the plain ESP32. Fix the error, then re-run it.

Expected binary size is about 540 KB; the app partition is 2 MB.

## Test procedure

Work through these in order. Each step says what to do, what should happen, and
what to send us if it does not. Steps 1 to 3 are the important ones.

### 1. Boot and advertise

Watch the monitor after flashing.

Expect, among the normal ESP-IDF boot lines:

```
I (...) ble_link: address xx:xx:xx:xx:xx:xx
I (...) ble_link: advertising as "CAIPOxxxx"
I (...) main: CAIPO firmware up: BLE v0 (contract of 15 Oct 2026)
```

`xxxx` is four hex digits from the Bluetooth address; the name is different on
every board.

If you see `main: BLE unavailable (...)` instead, the controller failed to
start. Send the full boot log from the first `ESP-ROM` line.

If the board resets in a loop or prints `Guru Meditation Error`, send the
backtrace. Run `idf.py monitor` again and it decodes addresses to source
lines automatically.

### 2. Connect and pair

In nRF Connect, scan. The device appears as `CAIPOxxxx` and the advertisement
shows a 128-bit service UUID starting `A1000000`. Tap Connect.

Expect a system pairing prompt on the phone (Just Works, no PIN). Accept it.

Expect in the log:

```
I (...) ble_link: connected handle=0
I (...) ble_link: link encrypted=1 authenticated=0 bonded=1
I (...) ble_link: MTU 517, chunk payload 509      (phone dependent)
I (...) main: link encrypted, MTU ..., chunk payload ...
```

An MTU lower than 517 is fine on older phones; the chunk payload just shrinks.

If pairing fails, send the log lines after `connected` and the phone's error
text. Also tell us the phone model and OS version.

### 3. Heartbeat

In nRF Connect, open the service `A1000000-...`, find characteristic
`A1000004`, and enable notifications (the triple-arrow icon).

Expect a two-byte notification every second: `10 00`, `10 01`, `10 02` ...
The second byte counts up and wraps at 255.

If nothing arrives, check the log for `subscriptions: data=0 ctrl=1
battery=0` (the subscribe was received) and for `heartbeat notify failed`.

### 4. Codec ID

Read characteristic `A1000002`. Expect one byte: `15`.

### 5. Status command

With notifications still enabled on `A1000004`, write one byte `20` to
`A1000004` (choose "BYTE ARRAY" in nRF Connect).

Expect a six-byte notification `21 00 64 00 00 00`: state STANDBY, battery
100 %, no flags, no pending uploads.

### 6. Short transfer from phone to device (optional)

Enable notifications on `A1000001` first. Then write these byte arrays to
`A1000003`, one at a time:

| Write to A1000003                           | Expect on A1000001 | Log |
|---------------------------------------------|--------------------|-----|
| `01 01 00`                                  | `02 01 00`         | transfer 1 REQUEST / READY |
| `03 01 00 05 00 00 00 03 86 A6 10 36 01 00` | nothing            | metadata accepted |
| `04 01 00 00 00 05 00 68 65 6C 6C 6F`       | `05 01 00`         | `transfer 1 complete: 5 bytes` |

The payload is "hello"; `86 A6 10 36` is its CRC-32 little-endian.

### 7. Touch pad sends a transfer

Tap the touch pad once.

Expect in the log `sending transfer 32769: 3000 bytes in ...` and, with
`A1000001` notifications enabled, a notification `01 01 80`. Without an app
answering READY, the log shows `transfer 32769 failed, error 0x05` after about
5 seconds. **That failure is correct behaviour** (nothing answered), not a bug.

### 8. Bond survives a reboot

Press the reset button on the board. Reconnect from nRF Connect.

Expect no new pairing prompt and `link encrypted=1 ... bonded=1` straight
away. If the phone asks to pair again, the bond was not persisted; send the
log.

### 9. Long press forgets bonds

Hold the touch pad for about 2 seconds. Expect in the log:

```
I (...) main: long press: forgetting all bonded phones
I (...) ble_link: bonds cleared rc=0
```

The phone is disconnected. The next connect asks to pair again. On the phone,
also "forget" the device in Bluetooth settings or it will refuse to re-pair.

### 10. The real app (if you have it)

Install `caipo-live.apk`, open it, pair. The app should show the heartbeat
indicator alive. Tap the touch pad: a transfer should appear on the phone.
Report what the app shows, with screenshots.

## What to send back

Whatever the outcome, send Sosina:

1. The full serial log, saved from the monitor, from the first `ESP-ROM` line
   to the end of your session.
2. Which steps passed and which did not.
3. Phone model and OS version.
4. Screenshots of anything that failed in nRF Connect or the app.

## Known limits of v0

- Battery level is a fixed 100 % placeholder.
- Touch tap sends a built-in 3000-byte test blob, not a real recording.
- Commands 0x22 to 0x29 (capture, record, stream) answer "not implemented"
  with a CMD_RESULT of UNSUPPORTED. They need the UART protocol v2 to the Pi.
- Only one phone connection at a time; up to three bonds are stored.

## Tunables

All in `idf.py menuconfig` under "Caipo BLE link": name prefix, heartbeat
period, supervision timeout, connection interval. `CONFIG_CAIPO_BLE_ENABLED`
must stay on for hardware; it is only switched off by the Wokwi build.
