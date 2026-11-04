#pragma once
#include <stddef.h>
#include <stdint.h>

// Diagnostics on the OLED: connection, last commands, counters.
// Events may be reported from any task, drawing only happens
// in diagLoop() (loop()).

// Find and start the display. false = no display found.
bool diagInit();

// true if the pin is used by the display.
bool diagPinUsed(int pin);

void diagConnected(const char* addr);
void diagDisconnected();

// Result of a command: executed (ST_OK, ST_EXEC_FAILED) or
// rejected (other status codes).
void diagCommand(uint8_t cmd, const uint8_t* payload, size_t len, uint8_t status);

void diagLoop();  // call from loop()

// Show two large lines (e.g. for tests at startup), serial only without a display.
void diagShowText(const char* line1, const char* line2);
