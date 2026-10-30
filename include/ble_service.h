#pragma once
#include <stddef.h>
#include <stdint.h>

// Protokoll Version 1 – Details siehe docs/ble-remote-protokoll.md
namespace proto {
constexpr uint8_t VERSION     = 0x01;
constexpr size_t  HEADER_LEN  = 2;   // Version + Befehl
constexpr size_t  NONCE_LEN   = 8;
constexpr size_t  MAC_LEN     = 8;
constexpr size_t  MAX_PAYLOAD = 10;
constexpr size_t  MAX_FRAME   = 20;

enum Status : uint8_t {
    ST_OK            = 0x00,
    ST_BAD_MAC       = 0x01,
    ST_NONCE_INVALID = 0x02,
    ST_UNKNOWN_CMD   = 0x03,
    ST_BAD_FORMAT    = 0x04,
    ST_EXEC_FAILED   = 0x05,
    ST_BUSY          = 0x06,  // Befehl angenommen, Ausführung läuft
    ST_NO_COMMAND    = 0xFF   // in dieser Verbindung noch kein Befehl
};
}  // namespace proto

void bleInit();
void bleLoop();  // in loop() aufrufen
