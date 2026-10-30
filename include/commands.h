#pragma once
#include <stddef.h>
#include <stdint.h>

// Hardware für die Befehle vorbereiten (wird in setup() aufgerufen).
void commandsInit();

// true, wenn es zur Befehlsnummer einen Handler gibt.
bool commandExists(uint8_t cmd);

// Führt den Befehl aus. Läuft in loop(), nicht im BLE-Task.
// Rückgabe false = Ausführung fehlgeschlagen (Status 0x05).
bool commandExecute(uint8_t cmd, const uint8_t* payload, size_t len);
