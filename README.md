# BLE Remote – ESP32 firmware

Counterpart for the Garmin Connect IQ widget **BLE ESP32 Remote Control**: the watch sends one of up to four commands over Bluetooth Low Energy, the ESP32 checks it and executes it (example: switch an LED on and off).

```
  Garmin watch (BLE central)                 ESP32 (BLE peripheral)
  Connect IQ widget        ── read nonce ──►  this firmware
                           ── command+HMAC ►  checks, executes (LED, relay, ...)
                           ◄── status ─────
```

- Commands can be protected with a shared key: HMAC-SHA256 (truncated to 8 bytes) over a one-time nonce, so commands can be neither faked nor replayed.
- Optional 1.3" OLED shows connection, last commands, counters and the reset reason.
- Protocol: [docs/protocol.md](docs/protocol.md) (German), with test vectors.
- Code comments are in German.

## Hardware

Tested with an **ESP32-C3 development board with 1.3" OLED** (TZT, SH1106 at I2C 0x3C, ceramic antenna, USB-C directly at the C3). Any ESP32-C3 should work; without an OLED the diagnostics only go to the serial monitor.

| Pin | Use on the TZT board |
|---|---|
| GPIO5 / GPIO6 | OLED SDA / SCL |
| GPIO8 | blue on-board LED (lights at LOW), used by the example commands |
| GPIO9 | BOOT button |

Tested watch: Forerunner 945.

## Build and flash

1. Install VS Code with the PlatformIO extension.
2. Open this folder, build and upload (`esp32c3-oled` environment).
3. Serial monitor: 115200 baud (USB CDC).

Bootloader mode if needed: hold BOOT, press RST, release BOOT.

## Configuration

All settings are in [include/config.h](include/config.h). Put your own values into `include/config_local.h` (ignored by git), it overrides the defaults. Template: [include/config_local.example.h](include/config_local.example.h).

```c
#define HMAC_KEY_HEX "…64 hex characters…"   // same key as in the widget
#define LED_PIN 8
#define LED_ON_HIGH 0
```

Generate a key:

```bash
python -c "import secrets; print(secrets.token_hex(32))"
```

Empty key = no security, for testing only (the OLED then shows `OFFEN!`).

In the widget settings (Garmin Connect app → device → Connect IQ apps → BLE Remote) enter the same key. Device name filter: `BLE-Remote`. The UUIDs must match `config.h`.

## Own commands

[src/commands.cpp](src/commands.cpp) contains the example: command 1 = LED on, command 2 = LED off. For your own actions either change that file, or write the same three functions (`commandsInit`, `commandExists`, `commandExecute`) in `src/commands_local.cpp`: the example functions are declared weak and are replaced automatically. `commands_local.cpp` is ignored by git.

## OLED diagnostics

```
BLE-Remote        HMAC     name, security (OFFEN! = no key)
Uhr dd:ee:ff        3s     connected watch, since when
──────────────────────
B2         OK      14s     newest command, inverted for 2 s
B1         OK      33s
OK 2 Fehl 0      0h10      counters, uptime
```

Before the first command it shows the boot counter and reset reason (e.g. `Start #12 STROM/1`). `BROWNOUT/9` means the supply dropped.

## Notes from testing

- **Brownouts:** the TZT board reset with BROWNOUT at full transmit power. A **22 µF** electrolytic capacitor between 3V and GND fixed it. 470 µF was too much: the OLED stayed dark after power-up.
- **Range:** about 6 m line of sight with the on-board antenna, no connection through a wall. A board with an external 2.4 GHz antenna works noticeably better. Transmit power is set to +21 dBm (`BLE_TX_DBM`).
- **Speed:** the Forerunner 945 switches the connection interval to 1000 ms shortly after connecting and rejects shorter ones. Reading the status then takes about 5 s, so the widget shows OK as soon as the write is acknowledged and reads the status in the background.
- **Nonce lifetime:** with a key, the watch needs more than 5 s from connecting to writing the command. The nonce is therefore valid for 30 s (`NONCE_TTL_MS`); with 5 s every command ended in a timeout. Each nonce is still single-use.
- **Device name:** the name is only in the scan response; the widget accepts devices without a name if the service UUID matches.
- **OLED driver:** the TZT board uses an SH1106 whose glass starts at column 0, so the SSD1306 driver gives the correct picture (`OLED_DRIVER 0`).
- **Finding the LED pin** on another board: set `LED_SEARCH 1`, every free pin blinks three times at startup and its number is shown on the OLED.

## License

[MIT](LICENSE)
