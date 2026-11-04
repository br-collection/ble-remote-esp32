// =====================================================================
//  Example actions: command 1 = LED on, command 2 = LED off.
//  Commands 1-4 are the menu entries in the widget.
//
//  Your own actions: either change them here, or write the same three functions
//  in a file of your own (e.g. src/commands_local.cpp). The
//  functions here are "weak" and are then replaced automatically.
//  payload holds the bytes entered in the widget (0-10 bytes).
//
//  Runs in loop(): short delays are fine, the watch polls the status
//  until execution has finished (mind the timeout in the widget).
// =====================================================================
#include "commands.h"

#include <Arduino.h>

#include "config.h"
#include "diag.h"

// false if the OLED uses the pin (the command then reports "FAILED")
static bool s_ledOk = false;

static void setLed(bool on) { digitalWrite(LED_PIN, on == LED_ON_HIGH ? HIGH : LOW); }

// Blink the free pins one after the other (LED_SEARCH)
static void ledSearch() {
    static const int8_t pins[] = {0, 1, 2, 3, 4, 7, 8, 10, 20, 21};
    char buf[12];
    for (int8_t p : pins) {
        if (diagPinUsed(p)) continue;
        snprintf(buf, sizeof(buf), "GPIO %d", p);
        diagShowText("LED search", buf);
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
        Serial.printf("WARNING: LED_PIN %d is used by the OLED, LED disabled\n", LED_PIN);
    }
}

__attribute__((weak)) bool commandExists(uint8_t cmd) { return cmd >= 1 && cmd <= 2; }

__attribute__((weak)) bool commandExecute(uint8_t cmd, const uint8_t* payload, size_t len) {
    if (!s_ledOk) return false;
    switch (cmd) {
        case 1:  // LED on (payload not needed)
            setLed(true);
            return true;

        case 2:  // LED off
            setLed(false);
            return true;
    }
    return false;
}
