# BLE Remote: protocol version 1

A Garmin watch (BLE central) sends one of 4 commands to a peripheral (e.g. an ESP32). All bytes in hex, multi-byte values big endian. Overview: [README](../README.md).

## GATT

| Characteristic | Property | Content |
|---|---|---|
| Nonce   | Read  | 8 random bytes, new on every connection and after every command attempt |
| Command | Write (with response) | command frame, max. 20 bytes |
| Status  | Read  | 1 byte result code |

Default UUIDs (can be changed in `firmware/include/config.h` and in the widget):

- Service: `71ee1368-aa5d-414c-b29c-b4bf2587a044`
- Nonce:   `4ca71f9b-9c77-43e4-9f3c-b4dd79231572`
- Command: `c311f60e-9476-457a-a8a9-ffbd7dbb81ff`
- Status:  `e3ca5a0a-479c-4cc3-9c63-3f88b70b5edc`

The advertising data contains the service UUID, the device name is in the scan response.

## Frame

With a key: `[version][command][payload 0-10 bytes][HMAC 8 bytes]`

Without a key: `[version][command][payload 0-10 bytes]`

- Version: `01`
- Command: `01`-`04`
- HMAC: the first 8 bytes of HMAC-SHA256(key, nonce ‖ version ‖ command ‖ payload)

The peripheral decides whether an HMAC is expected (key set or not).

## Sequence

1. Scan for the service UUID, connect
2. Read the nonce (only needed with a key)
3. Write the frame to Command
4. Read Status; while it is `06` or `FF`, read again shortly after

One connection can carry several commands: read a new nonce before each one. The widget does steps 1 and 2 in advance, as soon as it opens and after each command, so a selected command only has to write. See [widget.md](widget.md#speed).

## Status codes

| Code | Meaning |
|---|---|
| `00` | OK, executed |
| `01` | wrong HMAC |
| `02` | nonce missing or expired (read a new nonce) |
| `03` | unknown command |
| `04` | invalid format |
| `05` | execution failed |
| `06` | accepted, execution running |
| `FF` | no command yet on this connection |

## Limits of the firmware

- A nonce is valid for 30 s and for one attempt only (with 5 s the watch was too slow and every command ended in a timeout)
- The connection is dropped 20 s after connecting, or after 3 wrong HMACs
- Only one connection at a time

## Test vectors

Key: `000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f`

Nonce: `0102030405060708`

| Version, command, payload | HMAC (8 bytes) | Complete frame |
|---|---|---|
| `01 01` | `d1c00d0155cd7d1d` | `0101d1c00d0155cd7d1d` |
| `01 03 01f4` | `a6546bfa5e0db430` | `010301f4a6546bfa5e0db430` |

## Test with nRF Connect (without a key)

1. Leave `HMAC_KEY_HEX` empty, flash, open the serial monitor (115200 baud)
2. In nRF Connect, scan for "BLE-Remote" and connect
3. Write to Command (byte array): `0101` = LED on, `0102` = LED off (commands of the firmware: `firmware/src/commands.cpp`)
4. Read Status: `00` = OK
