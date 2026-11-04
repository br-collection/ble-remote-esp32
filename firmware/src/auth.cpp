#include "auth.h"

#include <mbedtls/md.h>
#include <string.h>

#include "config.h"

static uint8_t s_key[64];
static size_t  s_keyLen = 0;

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool authInit() {
    const char* hex = HMAC_KEY_HEX;
    size_t n = strlen(hex);
    if (n == 0) {
        s_keyLen = 0;
        return true;  // security deliberately disabled
    }
    if (n % 2 != 0 || n / 2 < 16 || n / 2 > sizeof(s_key)) return false;

    for (size_t i = 0; i < n / 2; i++) {
        int hi = hexVal(hex[2 * i]);
        int lo = hexVal(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        s_key[i] = (uint8_t)((hi << 4) | lo);
    }
    s_keyLen = n / 2;
    return true;
}

bool authEnabled() { return s_keyLen > 0; }

bool authVerify(const uint8_t* nonce, size_t nonceLen,
                const uint8_t* msg, size_t msgLen,
                const uint8_t* mac, size_t macLen) {
    if (!authEnabled() || macLen == 0 || macLen > 32) return false;

    uint8_t full[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    int rc = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (rc == 0) rc = mbedtls_md_hmac_starts(&ctx, s_key, s_keyLen);
    if (rc == 0) rc = mbedtls_md_hmac_update(&ctx, nonce, nonceLen);
    if (rc == 0) rc = mbedtls_md_hmac_update(&ctx, msg, msgLen);
    if (rc == 0) rc = mbedtls_md_hmac_finish(&ctx, full);
    mbedtls_md_free(&ctx);
    if (rc != 0) return false;

    // constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < macLen; i++) diff |= (uint8_t)(full[i] ^ mac[i]);
    return diff == 0;
}
