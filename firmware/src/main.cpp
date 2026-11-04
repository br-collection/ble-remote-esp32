#include <Arduino.h>
#include "auth.h"
#include "ble_service.h"
#include "commands.h"
#include "config.h"
#include "diag.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nBLE Remote starting ...");

    if (!authInit()) {
        Serial.println("ERROR: HMAC_KEY_HEX is invalid (16-64 bytes as hex).");
        Serial.println("Firmware stopped. Please fix config.h.");
        while (true) delay(1000);
    }
    if (!authEnabled()) {
        Serial.println("WARNING: no key set - commands are executed WITHOUT any check!");
    }

    diagInit();       // before commandsInit(): may occupy pins
    commandsInit();
    bleInit();
    Serial.println("Ready, advertising.");
}

void loop() {
    bleLoop();
    diagLoop();
    delay(10);
}
