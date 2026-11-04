#pragma once
// Template: copy to include/config_local.h and adjust.
// config_local.h is listed in .gitignore and never goes into the repository.
// Anything not set here uses the default from config.h.

// Shared key, same value as in the widget (hex, 16-64 bytes).
// Generate: python -c "import secrets; print(secrets.token_hex(32))"
// #define HMAC_KEY_HEX "...64 hex characters..."

// #define DEVICE_NAME "BLE-Remote"
// #define LED_PIN 8
// #define LED_ON_HIGH 0
