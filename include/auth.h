#pragma once
#include <stddef.h>
#include <stdint.h>

// Liest HMAC_KEY_HEX ein. false = Schlüssel ungültig formatiert.
bool authInit();

// true, wenn ein Schlüssel gesetzt ist (HMAC-Prüfung aktiv).
bool authEnabled();

// Prüft HMAC-SHA256(key, nonce || msg), gekürzt auf macLen Byte.
bool authVerify(const uint8_t* nonce, size_t nonceLen,
                const uint8_t* msg, size_t msgLen,
                const uint8_t* mac, size_t macLen);
