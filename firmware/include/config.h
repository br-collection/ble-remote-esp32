#pragma once
// =====================================================================
//  BLE Remote - configuration
//  This is the only file you need to adjust for your own setup.
//
//  Put your own values (key, pins, name) into include/config_local.h:
//  that file is listed in .gitignore, is read first here and overrides
//  the defaults below.
//  Template: include/config_local.example.h
// =====================================================================

#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ----- Device --------------------------------------------------------
#ifndef DEVICE_NAME
#define DEVICE_NAME "BLE-Remote"
#endif

// ----- UUIDs ---------------------------------------------------------
// Must match the settings in the Garmin widget exactly.
// Feel free to generate new UUIDs for your own installation.
#ifndef SERVICE_UUID
#define SERVICE_UUID "71ee1368-aa5d-414c-b29c-b4bf2587a044"
#endif
#ifndef NONCE_UUID
#define NONCE_UUID   "4ca71f9b-9c77-43e4-9f3c-b4dd79231572"
#endif
#ifndef COMMAND_UUID
#define COMMAND_UUID "c311f60e-9476-457a-a8a9-ffbd7dbb81ff"
#endif
#ifndef STATUS_UUID
#define STATUS_UUID  "e3ca5a0a-479c-4cc3-9c63-3f88b70b5edc"
#endif

// ----- Security ------------------------------------------------------
// Shared key as a hex string, 32 to 128 hex characters
// (= 16 to 64 bytes). Recommended: 64 hex characters (32 bytes).
// The same value is entered in the widget.
//
// Leave empty ("") = NO security. For testing only!
#ifndef HMAC_KEY_HEX
#define HMAC_KEY_HEX ""
#endif

// ----- Timing --------------------------------------------------------
#ifndef NONCE_TTL_MS
#define NONCE_TTL_MS        30000  // nonce lifetime; 5 s is not enough for the watch
#endif
#ifndef SESSION_TIMEOUT_MS
#define SESSION_TIMEOUT_MS 20000   // connection is dropped after this (FR945: 1 s interval)
#endif
#ifndef MAX_FAILED_ATTEMPTS
#define MAX_FAILED_ATTEMPTS    3   // wrong HMACs per connection, then disconnect
#endif

// ----- Radio ---------------------------------------------------------
// Transmit power in dBm, ESP32-C3: -24 to 21 (the library default is
// lower). Higher = more range to the watch, but more current: lower it
// again on brownouts (boot counter on the OLED).
#ifndef BLE_TX_DBM
#define BLE_TX_DBM 21
#endif
// Advertising interval in 0.625 ms: shorter = the watch finds the ESP32 faster
#ifndef ADV_INTERVAL_MIN
#define ADV_INTERVAL_MIN 32           // 20 ms
#endif
#ifndef ADV_INTERVAL_MAX
#define ADV_INTERVAL_MAX 48           // 30 ms
#endif
// Connection interval in 1.25 ms, requested after connecting
#ifndef CONN_INTERVAL_MIN
#define CONN_INTERVAL_MIN 6           // 7.5 ms
#endif
#ifndef CONN_INTERVAL_MAX
#define CONN_INTERVAL_MAX 24          // 30 ms
#endif
#ifndef CONN_SUPERVISION_TIMEOUT
#define CONN_SUPERVISION_TIMEOUT 400  // 4 s, in 10 ms
#endif
// Request again if the watch sets a longer interval. The FR945 still
// switches to 1000 ms and only answers after seconds: hence 0.
#ifndef CONN_PARAM_RETRIES
#define CONN_PARAM_RETRIES 0
#endif

// ----- OLED (diagnostics, see diag.cpp) ------------------------------
// I2C pins of the display (from the pinout of the TZT board).
// -1 = search at startup; the pins found are printed on the serial
// monitor and remembered in flash (for other boards).
#ifndef OLED_SDA
#define OLED_SDA 5
#endif
#ifndef OLED_SCL
#define OLED_SCL 6
#endif
// Driver (U8g2 sends the same init sequence for both, the only difference
// is the first column):
//   0 = SSD1306, glass starts at column 0 (TZT board, although the chip is an SH1106)
//   1 = SH1106, glass starts at column 2 (common 1.3" modules). On the TZT
//       board this shifts the image 2 pixels to the right, with garbage on the left.
#ifndef OLED_DRIVER
#define OLED_DRIVER 0
#endif

// ----- Hardware for the commands (see commands.cpp) ------------------
// ESP32-C3: free pins are GPIO 0-10, 20, 21 (9 = button BO0). Pins used
// by the OLED are blocked for commands.
// Command 1 = LED on, command 2 = LED off.
#ifndef LED_PIN
#define LED_PIN 8
#endif
// 1 = LED lights at HIGH, 0 = at LOW. TZT board: blue LED on GPIO8,
// lights at LOW.
#ifndef LED_ON_HIGH
#define LED_ON_HIGH 0
#endif
// 1 = blink every free pin 3 times at startup, the pin number is shown on
// the OLED. For finding the LED on a board, set back to 0 afterwards.
#ifndef LED_SEARCH
#define LED_SEARCH 0
#endif
