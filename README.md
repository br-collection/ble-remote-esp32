# BLE ESP32 Remote

Control an ESP32 from your Garmin watch: a Connect IQ widget sends one of up to four commands over Bluetooth Low Energy, the ESP32 checks it and executes it. The included example switches an LED on and off.

```
  Garmin watch (BLE central)                 ESP32 (BLE peripheral)
  widget/                  ── read nonce ──►  firmware/
                           ── command+HMAC ►  checks, executes (LED, relay, ...)
                           ◄── status ─────
```

- Up to four commands with your own names and optional payload.
- Commands can be protected with a shared key: HMAC-SHA256 (truncated to 8 bytes) over a one-time nonce, so commands can be neither faked nor replayed.
- The widget connects as soon as it opens and keeps the connection, so a command does not have to scan and connect first.
- Optional 1.3" OLED on the ESP32 shows the connection, last commands, counters and the reset reason.

## Contents

| Folder | Content |
|---|---|
| [`firmware/`](firmware) | ESP32-C3 firmware (PlatformIO, Arduino, NimBLE) |
| [`widget/`](widget) | Garmin Connect IQ widget (Monkey C) |
| [`docs/firmware.md`](docs/firmware.md) | hardware, build, configuration, own commands, OLED |
| [`docs/widget.md`](docs/widget.md) | build, settings, usage, speed |
| [`docs/connect-iq-setup.md`](docs/connect-iq-setup.md) | installing the Connect IQ tools |
| [`docs/protocol.md`](docs/protocol.md) | GATT, frame, status codes, test vectors |

## Quick start

1. **Firmware:** open `firmware` with PlatformIO, build and upload to an ESP32-C3. Set `LED_PIN` and `LED_ON_HIGH` for your board, see [firmware.md](docs/firmware.md).
2. **Widget:** open `widget` in VS Code with the Monkey C extension, run **Monkey C: Build for Device** and copy the `.prg` to `GARMIN/APPS` on the watch, see [widget.md](docs/widget.md).
3. **Try it:** open the widget, press START, choose **LED On** or **LED Off**.

This works without a key, which is for testing only: anyone in range can then send commands.

## Adding a key

Generate a key:

```bash
python -c "import secrets; print(secrets.token_hex(32))"
```

Enter the same value on both sides:

- ESP32: `#define HMAC_KEY_HEX "..."` in `firmware/include/config_local.h` (ignored by git).
- Widget: the setting "Key" in the Garmin Connect app, or the default `key` in `widget/resources/properties/properties.xml` if you sideload the widget. **Do not commit a real key.**

## Tested

| Part | Status |
|---|---|
| Forerunner 945 with an ESP32-C3 | commands with and without a key on the real watch |
| Forerunner 55 | tests of the widget's own HMAC pass in the simulator |
| Forerunner 945 LTE, vívoactive 4 / 4S | builds only |

## License

[PolyForm Noncommercial 1.0.0](LICENSE): anyone may use, change and republish this project for private and other noncommercial purposes. Keep the copyright notice when you pass it on. Commercial use is not covered; please ask first.

Versions up to commit `5b31382` were published under the MIT license and stay available under it.
