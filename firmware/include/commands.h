#pragma once
#include <stddef.h>
#include <stdint.h>

// Prepare the hardware for the commands (called from setup()).
void commandsInit();

// true if there is a handler for this command number.
bool commandExists(uint8_t cmd);

// Executes the command. Runs in loop(), not in the BLE task.
// Returns false = execution failed (status 0x05).
bool commandExecute(uint8_t cmd, const uint8_t* payload, size_t len);
