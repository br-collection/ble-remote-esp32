#pragma once
// =====================================================================
//  BLE-Remote – Konfiguration
//  Nur diese Datei muss für den eigenen Einsatz angepasst werden.
//
//  Eigene Werte (Schlüssel, Pins, Name) am besten in
//  include/config_local.h eintragen: Die Datei ist in .gitignore, wird
//  hier zuerst eingelesen und überschreibt die Standardwerte unten.
//  Vorlage: include/config_local.example.h
// =====================================================================

#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ----- Gerät ---------------------------------------------------------
#ifndef DEVICE_NAME
#define DEVICE_NAME "BLE-Remote"
#endif

// ----- UUIDs ---------------------------------------------------------
// Müssen exakt mit den Einstellungen im Garmin-Widget übereinstimmen.
// Für eigene Installationen gern neue UUIDs erzeugen.
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

// ----- Sicherheit ----------------------------------------------------
// Gemeinsamer Schlüssel als Hex-String, 32 bis 128 Hex-Zeichen
// (= 16 bis 64 Byte). Empfohlen: 64 Hex-Zeichen (32 Byte).
// Derselbe Wert wird im Widget eingetragen.
//
// Leer lassen ("") = KEINE Sicherheit. Nur zum Testen verwenden!
#ifndef HMAC_KEY_HEX
#define HMAC_KEY_HEX ""
#endif

// ----- Zeiten --------------------------------------------------------
#ifndef NONCE_TTL_MS
#define NONCE_TTL_MS        30000  // Gültigkeit einer Nonce; 5 s reichen der Uhr nicht
#endif
#ifndef SESSION_TIMEOUT_MS
#define SESSION_TIMEOUT_MS 20000   // Verbindung wird danach getrennt (FR945: 1-s-Intervall)
#endif
#ifndef MAX_FAILED_ATTEMPTS
#define MAX_FAILED_ATTEMPTS    3   // falsche HMACs pro Verbindung, dann Trennen
#endif

// ----- Funk ----------------------------------------------------------
// Sendeleistung in dBm, ESP32-C3: -24 bis 21 (Standard der Bibliothek ist
// niedriger). Höher = größere Reichweite zur Uhr, aber mehr Strom: bei
// Brownouts (Startzähler auf dem OLED) wieder senken.
#ifndef BLE_TX_DBM
#define BLE_TX_DBM 21
#endif
// Advertising-Intervall in 0,625 ms: kürzer = Uhr findet den ESP32 schneller
#ifndef ADV_INTERVAL_MIN
#define ADV_INTERVAL_MIN 32           // 20 ms
#endif
#ifndef ADV_INTERVAL_MAX
#define ADV_INTERVAL_MAX 48           // 30 ms
#endif
// Verbindungsintervall in 1,25 ms, nach dem Verbinden angefordert
#ifndef CONN_INTERVAL_MIN
#define CONN_INTERVAL_MIN 6           // 7,5 ms
#endif
#ifndef CONN_INTERVAL_MAX
#define CONN_INTERVAL_MAX 24          // 30 ms
#endif
#ifndef CONN_SUPERVISION_TIMEOUT
#define CONN_SUPERVISION_TIMEOUT 400  // 4 s, in 10 ms
#endif
// Nachfordern, wenn die Uhr ein längeres Intervall setzt. Die FR945 stellt
// trotzdem auf 1000 ms und antwortet erst nach Sekunden: daher 0.
#ifndef CONN_PARAM_RETRIES
#define CONN_PARAM_RETRIES 0
#endif

// ----- OLED (Diagnose, siehe diag.cpp) -------------------------------
// I2C-Pins des Displays (laut Pinbelegung des TZT-Boards).
// -1 = beim Start suchen; die gefundenen Pins stehen im seriellen
// Monitor und werden im Flash gemerkt (für andere Boards).
#ifndef OLED_SDA
#define OLED_SDA 5
#endif
#ifndef OLED_SCL
#define OLED_SCL 6
#endif
// Treiber (U8g2 schickt für beide dieselbe Init-Sequenz, Unterschied ist
// nur die erste Spalte):
//   0 = SSD1306, Glas ab Spalte 0 (TZT-Board, obwohl der Chip ein SH1106 ist)
//   1 = SH1106, Glas ab Spalte 2 (übliche 1,3"-Module). Auf dem TZT-Board
//       2 Pixel nach rechts verschoben, links Pixelmüll.
#ifndef OLED_DRIVER
#define OLED_DRIVER 0
#endif

// ----- Hardware für die Befehle (siehe commands.cpp) -----------------
// ESP32-C3: frei sind GPIO 0-10, 20, 21 (9 = Taster BO0). Pins, die das
// OLED belegt, werden für Befehle gesperrt.
// Befehl 1 = LED ein, Befehl 2 = LED aus.
#ifndef LED_PIN
#define LED_PIN 8
#endif
// 1 = LED leuchtet bei HIGH, 0 = bei LOW. TZT-Board: blaue LED an GPIO8
// (per LED_SEARCH gefunden, 2026-10-30), leuchtet bei LOW.
#ifndef LED_ON_HIGH
#define LED_ON_HIGH 0
#endif
// 1 = beim Start jeden freien Pin 3x blinken lassen, Pin steht auf dem
// OLED. Zum Finden der LED auf dem Board, danach wieder 0.
#ifndef LED_SEARCH
#define LED_SEARCH 0
#endif
