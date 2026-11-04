# ESP32 firmware

PlatformIO project in [`firmware/`](../firmware), Arduino framework with NimBLE.

## Hardware

Tested with an **ESP32-C3 development board with 1.3" OLED** (TZT, SH1106 at I2C 0x3C, ceramic antenna, USB-C directly at the C3). Any ESP32-C3 should work; without an OLED the diagnostics only go to the serial monitor.

| Pin | Use on the TZT board |
|---|---|
| GPIO5 / GPIO6 | OLED SDA / SCL |
| GPIO8 | blue on-board LED (lights at LOW), used by the example commands |
| GPIO9 | BOOT button |

## Build and flash

1. Install VS Code with the PlatformIO extension.
2. Open the folder `firmware`, build and upload (`esp32c3-oled` environment).
3. Serial monitor: 115200 baud (USB CDC).

Bootloader mode if needed: hold BOOT, press RST, release BOOT.

## Configuration

All settings are in [`firmware/include/config.h`](../firmware/include/config.h). Put your own values into `firmware/include/config_local.h` (ignored by git), it overrides the defaults. Template: [`config_local.example.h`](../firmware/include/config_local.example.h).

```c
#define HMAC_KEY_HEX "...64 hex characters..."   // same key as in the widget
#define LED_PIN 8
#define LED_ON_HIGH 0
```

Empty key = no security, for testing only (the OLED then shows `OPEN!`).

## Own commands

[`firmware/src/commands.cpp`](../firmware/src/commands.cpp) contains the example: command 1 = LED on, command 2 = LED off. For your own actions either change that file, or write the same three functions (`commandsInit`, `commandExists`, `commandExecute`) in `firmware/src/commands_local.cpp`: the example functions are declared weak and are replaced automatically. `commands_local.cpp` is ignored by git.

## OLED diagnostics

```
BLE-Remote        HMAC     name, security (OPEN! = no key)
Watch dd:ee:ff      3s     connected watch, since when
----------------------
C2         OK      14s     newest command, inverted for 2 s
C1         OK      33s
OK 2 Err 0        0h10     counters, uptime
```

Before the first command it shows the boot counter and reset reason (e.g. `Boot #12 POWER/1`). `BROWNOUT/9` means the supply dropped.

## Code

| File | Content |
|---|---|
| `include/config.h` | all settings: UUIDs, key, timing, radio, pins |
| `src/ble_service.cpp` | GATT server, nonce, frame check, handover to `loop()` |
| `src/auth.cpp` | parse the key, check HMAC-SHA256 (mbedTLS) |
| `src/commands.cpp` | example commands: LED on, LED off |
| `src/diag.cpp` | OLED diagnostics |

## Notes from testing

- **Brownouts:** the TZT board reset with BROWNOUT at full transmit power. A **22 µF** electrolytic capacitor between 3V and GND fixed it. 470 µF was too much: the OLED stayed dark after power-up. Alternatively lower `BLE_TX_DBM`.
- **Range:** about 6 m line of sight with the on-board antenna, no connection through a wall. A board with an external 2.4 GHz antenna works noticeably better. Transmit power is set to +21 dBm (`BLE_TX_DBM`).
- **Nonce lifetime:** with a key, the watch needs more than 5 s from connecting to writing the command. The nonce is therefore valid for 30 s (`NONCE_TTL_MS`); with 5 s every command ended in a timeout. Each nonce is still single-use.
- **Device name:** the name is only in the scan response; the widget accepts devices without a name if the service UUID matches.
- **OLED driver:** the TZT board uses an SH1106 whose glass starts at column 0, so the SSD1306 driver gives the correct picture (`OLED_DRIVER 0`).
- **Finding the LED pin** on another board: set `LED_SEARCH 1`, every free pin blinks three times at startup and its number is shown on the OLED.
