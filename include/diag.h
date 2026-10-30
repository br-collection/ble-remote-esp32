#pragma once
#include <stddef.h>
#include <stdint.h>

// Diagnose auf dem OLED: Verbindung, letzte Befehle, Zähler.
// Ereignisse dürfen aus jedem Task gemeldet werden, gezeichnet wird nur
// in diagLoop() (loop()).

// Display suchen und starten. false = kein Display gefunden.
bool diagInit();

// true, wenn der Pin vom Display belegt ist.
bool diagPinUsed(int pin);

void diagConnected(const char* addr);
void diagDisconnected();

// Ergebnis eines Befehls: ausgeführt (ST_OK, ST_EXEC_FAILED) oder
// abgelehnt (übrige Statuscodes).
void diagCommand(uint8_t cmd, const uint8_t* payload, size_t len, uint8_t status);

void diagLoop();  // in loop() aufrufen

// Zwei Zeilen groß anzeigen (z. B. für Tests beim Start), ohne Display nur seriell.
void diagShowText(const char* line1, const char* line2);
