# Garmin widget

Connect IQ widget in Monkey C, in [`widget/`](../widget). If the toolchain is new to you, start with [connect-iq-setup.md](connect-iq-setup.md).

Watches in the manifest: Forerunner 945 / 945 LTE, Forerunner 55, vívoactive 4 / 4S (`minApiLevel` 3.3.0). Other watches with BLE support can be added with **Monkey C: Edit Products**.

## Build

Open the folder `widget` in VS Code, press `F5` for the simulator or run **Monkey C: Build for Device**. Copy the `.prg` to `GARMIN/APPS` on the watch over USB.

## Settings

| Setting | Meaning |
|---|---|
| Key | hex, 16-64 bytes, same as `HMAC_KEY_HEX` on the ESP32. Empty = no security |
| Device name filter | only devices with exactly this name. Empty = off |
| Scan timeout | 3-30 s |
| Command 1-4 | enabled, name in the menu, payload (hex, max. 10 bytes, e.g. `01F4`) |
| UUIDs | must match the peripheral, default = this firmware |

Defaults: command 1 "LED On", command 2 "LED Off", no payload; commands 3 and 4 off; name filter `BLE-Remote`; no key.

Settings can only be changed in the Garmin Connect app if the widget was installed from the Connect IQ Store (beta included). With a sideloaded `.prg` the defaults from [`widget/resources/properties/properties.xml`](../widget/resources/properties/properties.xml) apply. Enter your own values there, or use the app settings editor in the simulator.

**Never commit a real key.** With a key in `properties.xml`, keep that file out of your commits, for example with:

```bash
git update-index --skip-worktree widget/resources/properties/properties.xml
```

If the UUIDs are changed while the widget is running, it has to be reopened once.

## Usage

Opening the widget already connects in the background. START or a tap opens the menu. After choosing a command: Searching → Connecting → Sending → result with vibration (short = OK, long = error). BACK cancels.

"OK" appears as soon as the ESP32 has acknowledged the command (write response). The widget then reads the status in the background; if the ESP32 reports an error, the display switches to the error and the watch vibrates long.

If the connection is lost before the write, the widget reconnects and sends again. After the write it never sends again, so a command cannot be executed twice. If the connection is lost after the acknowledgement, "OK" stays with the addition `Status ?`.

### Timing display

Below "OK" the measured times are shown in seconds:

| Text | Meaning |
|---|---|
| `Sc` / `Co` | scanning / connecting |
| `linked` | the connection was already up |
| `Se` | sending in total: from connected to result |
| `Wr` | until the write was acknowledged |
| `Rd` | reading the status, `x1` = number of reads |

### Error display

"BLE error" and "Device not found" show where it happened, for example `R3 nonce` (read failed) or `W2 write` (write failed). After "Device not found": `D` = devices seen, `N` = with a name, `U` = matching service UUID, `P` = profile registered (`1` yes, `-` failed, `?` no answer yet).

## Speed

The connection is set up when the widget opens and stays open for 60 s after opening or after the last command. The firmware drops a connection 20 s after connecting, so the widget renews it after 14 s; a command during the short gap joins the new setup. The nonce is read in advance as well, so a command only has to write. Closing the widget disconnects.

The watch decides on the connection interval. The Forerunner 945 switches to 1000 ms shortly after connecting and rejects shorter ones, whatever the firmware requests, and a Connect IQ widget cannot change that. Measured there on an existing connection: about 2.5-3 s for a read or a write. Reading the status happens after "OK" is shown, so it does not delay the command.

## Memory and API differences

- The Forerunner 55 has only 64 KB of memory for widgets, the others 512 KB to 1 MB. Check with **File → View Memory** in the simulator.
- The firmware of the Forerunner 55 does not have the class `Cryptography.HashBasedMessageAuthenticationCode` at runtime, although the SDK lists it ("Symbol Not Found"). The widget then calculates HMAC-SHA256 itself in Monkey C (`Protocol.mc`).
- `onProfileRegister` does not arrive on the Forerunner 945, so the widget does not wait for it.

## Tests

`widget/source/ProtocolTest.mc` checks SHA-256 and HMAC against published test vectors. Build with `monkeyc -t`, start the simulator, then:

```bash
monkeydo bin/test.prg fr55 /t
```

## Code

| File | Content |
|---|---|
| `BleRemoteApp.mc` | entry point, load configuration, settings changes |
| `Config.mc` | read and check the settings |
| `CommandMenu.mc` | widget view and command menu |
| `BleClient.mc` | BLE sequence as a state machine with timeouts |
| `Protocol.mc` | frame, HMAC, hex parser, result codes |
| `ProtocolTest.mc` | tests for SHA-256 and HMAC |
| `ResultView.mc` | progress, result display, vibration |
