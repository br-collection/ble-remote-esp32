// =====================================================================
//  Beispiel-Aktionen: Befehl 1 = LED ein, Befehl 2 = LED aus.
//  Befehl 1-4 entsprechen den Menüeinträgen im Widget.
//
//  Eigene Aktionen: entweder hier ändern, oder dieselben drei Funktionen
//  in einer eigenen Datei (z. B. src/commands_local.cpp) schreiben. Die
//  Funktionen hier sind "weak" und werden dann automatisch ersetzt.
//  payload enthält die im Widget eingetragenen Bytes (0-10 Byte).
//
//  Läuft in loop(): kurze Wartezeiten sind ok, die Uhr fragt den Status
//  so lange ab, bis die Ausführung fertig ist (Timeout im Widget beachten).
// =====================================================================
#include "commands.h"

#include <Arduino.h>

#include "config.h"
#include "diag.h"

// false, wenn das OLED den Pin belegt (Befehl meldet dann "FEHLER")
static bool s_ledOk = false;

static void setLed(bool on) { digitalWrite(LED_PIN, on == LED_ON_HIGH ? HIGH : LOW); }

// Freie Pins nacheinander blinken lassen (LED_SEARCH)
static void ledSearch() {
    static const int8_t pins[] = {0, 1, 2, 3, 4, 7, 8, 10, 20, 21};
    char buf[12];
    for (int8_t p : pins) {
        if (diagPinUsed(p)) continue;
        snprintf(buf, sizeof(buf), "GPIO %d", p);
        diagShowText("LED-Suche", buf);
        pinMode(p, OUTPUT);
        for (int i = 0; i < 3; i++) {
            digitalWrite(p, HIGH);
            delay(300);
            digitalWrite(p, LOW);
            delay(300);
        }
        pinMode(p, INPUT);
        delay(400);
    }
}

__attribute__((weak)) void commandsInit() {
#if LED_SEARCH
    ledSearch();
#endif
    s_ledOk = !diagPinUsed(LED_PIN);
    if (s_ledOk) {
        pinMode(LED_PIN, OUTPUT);
        setLed(false);
    } else {
        Serial.printf("WARNUNG: LED_PIN %d belegt das OLED, LED gesperrt\n", LED_PIN);
    }
}

__attribute__((weak)) bool commandExists(uint8_t cmd) { return cmd >= 1 && cmd <= 2; }

__attribute__((weak)) bool commandExecute(uint8_t cmd, const uint8_t* payload, size_t len) {
    if (!s_ledOk) return false;
    switch (cmd) {
        case 1:  // LED ein (Payload wird nicht gebraucht)
            setLed(true);
            return true;

        case 2:  // LED aus
            setLed(false);
            return true;
    }
    return false;
}
