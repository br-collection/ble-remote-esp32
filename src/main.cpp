#include <Arduino.h>
#include "auth.h"
#include "ble_service.h"
#include "commands.h"
#include "config.h"
#include "diag.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nBLE-Remote startet ...");

    if (!authInit()) {
        Serial.println("FEHLER: HMAC_KEY_HEX ist ungueltig (16-64 Byte als Hex).");
        Serial.println("Firmware angehalten. Bitte config.h korrigieren.");
        while (true) delay(1000);
    }
    if (!authEnabled()) {
        Serial.println("WARNUNG: Kein Schluessel gesetzt - Befehle werden OHNE Pruefung ausgefuehrt!");
    }

    diagInit();       // vor commandsInit(): belegt ggf. Pins
    commandsInit();
    bleInit();
    Serial.println("Bereit, Advertising laeuft.");
}

void loop() {
    bleLoop();
    diagLoop();
    delay(10);
}
