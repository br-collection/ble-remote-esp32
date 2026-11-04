#pragma once
#include <stddef.h>
#include <stdint.h>

// Parses HMAC_KEY_HEX. false = key has an invalid format.
bool authInit();

// true if a key is set (HMAC check active).
bool authEnabled();

// Checks HMAC-SHA256(key, nonce || msg), truncated to macLen bytes.
bool authVerify(const uint8_t* nonce, size_t nonceLen,
                const uint8_t* msg, size_t msgLen,
                const uint8_t* mac, size_t macLen);
