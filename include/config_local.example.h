#pragma once
// Vorlage: nach include/config_local.h kopieren und anpassen.
// config_local.h ist in .gitignore und kommt nicht ins Repository.
// Alles, was hier nicht steht, nimmt den Standardwert aus config.h.

// Gemeinsamer Schlüssel, derselbe Wert wie im Widget (Hex, 16-64 Byte).
// Erzeugen: python -c "import secrets; print(secrets.token_hex(32))"
// #define HMAC_KEY_HEX "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"

// #define DEVICE_NAME "BLE-Remote"
// #define LED_PIN 8
// #define LED_ON_HIGH 0
