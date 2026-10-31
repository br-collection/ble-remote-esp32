# BLE-Remote – Protokoll Version 1

Garmin-Uhr (BLE-Central) sendet einen von 4 Befehlen an eine Gegenstelle
(BLE-Peripheral, z. B. ESP32). Alle Bytes in Hex, Mehrbyte-Werte Big Endian.
Übersicht: [README](../README.md).

## GATT

| Characteristic | Eigenschaft | Inhalt |
|---|---|---|
| Nonce   | Read  | 8 Byte Zufallswert, neu bei jeder Verbindung und nach jedem Befehlsversuch |
| Command | Write (mit Response) | Befehls-Frame, max. 20 Byte |
| Status  | Read  | 1 Byte Ergebniscode |

Standard-UUIDs (in `config.h` und im Widget änderbar):

- Service: `71ee1368-aa5d-414c-b29c-b4bf2587a044`
- Nonce:   `4ca71f9b-9c77-43e4-9f3c-b4dd79231572`
- Command: `c311f60e-9476-457a-a8a9-ffbd7dbb81ff`
- Status:  `e3ca5a0a-479c-4cc3-9c63-3f88b70b5edc`

Advertising enthält die Service-UUID, der Gerätename steht in der Scan-Response.

## Frame

Mit Schlüssel:  `[Version][Befehl][Payload 0–10 B][HMAC 8 B]`
Ohne Schlüssel: `[Version][Befehl][Payload 0–10 B]`

- Version: `01`
- Befehl: `01`–`04`
- HMAC: die ersten 8 Byte von HMAC-SHA256(Schlüssel, Nonce ‖ Version ‖ Befehl ‖ Payload)

Ob ein HMAC erwartet wird, legt die Gegenstelle fest (Schlüssel gesetzt oder nicht).

## Ablauf

1. Scannen nach Service-UUID, verbinden
2. Nonce lesen (nur mit Schlüssel nötig)
3. Frame auf Command schreiben
4. Status lesen; solange `06`, nach ca. 200 ms erneut lesen
5. Trennen

## Statuscodes

| Code | Bedeutung |
|---|---|
| `00` | OK, ausgeführt |
| `01` | HMAC falsch |
| `02` | Nonce fehlt oder abgelaufen (Nonce neu lesen) |
| `03` | unbekannter Befehl |
| `04` | Format falsch |
| `05` | Ausführung fehlgeschlagen |
| `06` | angenommen, Ausführung läuft |
| `FF` | in dieser Verbindung noch kein Befehl |

## Grenzen der Gegenstelle

- Nonce gültig 30 s, jede Nonce nur für einen Versuch (mit 5 s war die Uhr zu langsam: Zeitüberschreitung)
- Verbindung wird nach 10 s oder nach 3 falschen HMACs getrennt
- Nur eine Verbindung gleichzeitig

## Testvektoren

Schlüssel: `000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f`
Nonce: `0102030405060708`

| Version, Befehl, Payload | HMAC (8 B) | kompletter Frame |
|---|---|---|
| `01 01` | `d1c00d0155cd7d1d` | `0101d1c00d0155cd7d1d` |
| `01 03 01f4` | `a6546bfa5e0db430` | `010301f4a6546bfa5e0db430` |

## Test mit nRF Connect (ohne Schlüssel)

1. In `config.h` `HMAC_KEY_HEX ""` lassen, flashen, seriellen Monitor öffnen (115200 Baud)
2. In nRF Connect nach „BLE-Remote“ scannen und verbinden
3. Auf Command schreiben (Byte Array): `0101` → LED ein, `0102` → LED aus
   (Belegung der Firmware in `src/commands.cpp`)
4. Status lesen: `00` = OK
