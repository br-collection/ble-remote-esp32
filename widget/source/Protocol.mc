// BLE Remote - protocol version 1 (see docs/protocol.md)
import Toybox.Cryptography;
import Toybox.Lang;

// Progress of a transfer
enum {
    PHASE_SCAN,
    PHASE_CONNECT,
    PHASE_SEND
}

// Result of a transfer
enum {
    RES_OK,
    RES_NOT_FOUND,
    RES_CONNECT_FAILED,
    RES_BAD_KEY,
    RES_UNKNOWN_CMD,
    RES_BAD_FORMAT,
    RES_EXEC_FAILED,
    RES_TIMEOUT,
    RES_BLE_ERROR,
    RES_CANCELLED
}

// Base class for receivers of progress and result
class BleListener {
    function initialize() {}
    function onBleProgress(phase as Number) as Void {}
    // ESP32 received the command (write acknowledged), status still to come
    function onBleSent() as Void {}
    function onBleResult(result as Number) as Void {}
}

module Protocol {
    const VERSION = 0x01;
    const NONCE_LEN = 8;
    const MAC_LEN = 8;
    const MAX_PAYLOAD = 10;

    // Status codes of the peripheral
    const ST_OK = 0x00;
    const ST_BAD_MAC = 0x01;
    const ST_NONCE_INVALID = 0x02;
    const ST_UNKNOWN_CMD = 0x03;
    const ST_BAD_FORMAT = 0x04;
    const ST_EXEC_FAILED = 0x05;
    const ST_BUSY = 0x06;
    const ST_NO_COMMAND = 0xFF;

    // Frame: [version][command][payload][HMAC 8 bytes, only with a key]
    // HMAC = HMAC-SHA256(key, nonce || version || command || payload), truncated
    function buildFrame(cmd as Number, payload as ByteArray,
                        key as ByteArray?, nonce as ByteArray?) as ByteArray {
        var msg = new [2]b;
        msg[0] = VERSION;
        msg[1] = cmd;
        msg = msg.addAll(payload);

        if (key != null && nonce != null) {
            var data = nonce.slice(0, null);
            data = data.addAll(msg);
            msg = msg.addAll(hmacSha256(key, data).slice(0, MAC_LEN));
        }
        return msg;
    }

    // HMAC-SHA256. Some watches (e.g. FR55) do not have the class
    // HashBasedMessageAuthenticationCode although the SDK lists it
    // ("Symbol Not Found"); sha256() below then does the work in Monkey C.
    function hmacSha256(key as ByteArray, data as ByteArray) as ByteArray {
        if (Cryptography has :HashBasedMessageAuthenticationCode) {
            var hmac = new Cryptography.HashBasedMessageAuthenticationCode({
                :algorithm => Cryptography.HASH_SHA256,
                :key => key
            });
            hmac.update(data);
            return hmac.digest();
        }
        return hmacSoft(key, data);
    }

    // HMAC-SHA256 entirely in Monkey C (RFC 2104)
    function hmacSoft(key as ByteArray, data as ByteArray) as ByteArray {
        if (key.size() > 64) {
            key = sha256(key);
        }
        var inner = new [64]b;
        var outer = new [64]b;
        for (var i = 0; i < 64; i++) {
            var k = i < key.size() ? key[i] : 0;
            inner[i] = k ^ 0x36;
            outer[i] = k ^ 0x5c;
        }
        inner = inner.addAll(data);
        outer = outer.addAll(sha256(inner));
        return sha256(outer);
    }

    // SHA-256 in Monkey C. Uses Long with a mask because Number
    // is only 32 bits and signed. Fast enough for the few bytes
    // of a command.
    const M32 = 0xFFFFFFFFl;

    function rotr(x as Long, n as Number) as Long {
        return ((x >> n) | (x << (32 - n))) & M32;
    }

    function sha256(msg as ByteArray) as ByteArray {
        var k = [
            0x428a2f98l, 0x71374491l, 0xb5c0fbcfl, 0xe9b5dba5l, 0x3956c25bl, 0x59f111f1l, 0x923f82a4l, 0xab1c5ed5l,
            0xd807aa98l, 0x12835b01l, 0x243185bel, 0x550c7dc3l, 0x72be5d74l, 0x80deb1fel, 0x9bdc06a7l, 0xc19bf174l,
            0xe49b69c1l, 0xefbe4786l, 0x0fc19dc6l, 0x240ca1ccl, 0x2de92c6fl, 0x4a7484aal, 0x5cb0a9dcl, 0x76f988dal,
            0x983e5152l, 0xa831c66dl, 0xb00327c8l, 0xbf597fc7l, 0xc6e00bf3l, 0xd5a79147l, 0x06ca6351l, 0x14292967l,
            0x27b70a85l, 0x2e1b2138l, 0x4d2c6dfcl, 0x53380d13l, 0x650a7354l, 0x766a0abbl, 0x81c2c92el, 0x92722c85l,
            0xa2bfe8a1l, 0xa81a664bl, 0xc24b8b70l, 0xc76c51a3l, 0xd192e819l, 0xd6990624l, 0xf40e3585l, 0x106aa070l,
            0x19a4c116l, 0x1e376c08l, 0x2748774cl, 0x34b0bcb5l, 0x391c0cb3l, 0x4ed8aa4al, 0x5b9cca4fl, 0x682e6ff3l,
            0x748f82eel, 0x78a5636fl, 0x84c87814l, 0x8cc70208l, 0x90befffal, 0xa4506cebl, 0xbef9a3f7l, 0xc67178f2l
        ];
        var h = [
            0x6a09e667l, 0xbb67ae85l, 0x3c6ef372l, 0xa54ff53al,
            0x510e527fl, 0x9b05688cl, 0x1f83d9abl, 0x5be0cd19l
        ];

        // Padding: 0x80, zeros, length in bits (64 bits, big endian)
        var len = msg.size();
        var padded = msg.slice(0, null);
        padded = padded.add(0x80);
        while (padded.size() % 64 != 56) {
            padded = padded.add(0);
        }
        var bits = len.toLong() * 8;
        for (var i = 7; i >= 0; i--) {
            padded = padded.add(((bits >> (i * 8)) & 0xFF).toNumber());
        }

        var w = new [64];
        for (var blk = 0; blk < padded.size(); blk += 64) {
            for (var t = 0; t < 16; t++) {
                var p = blk + t * 4;
                w[t] = (padded[p].toLong() << 24) | (padded[p + 1] << 16)
                     | (padded[p + 2] << 8) | padded[p + 3];
            }
            for (var t = 16; t < 64; t++) {
                var x = w[t - 15];
                var y = w[t - 2];
                var s0 = rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
                var s1 = rotr(y, 17) ^ rotr(y, 19) ^ (y >> 10);
                w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & M32;
            }
            var a = h[0];
            var b = h[1];
            var c = h[2];
            var d = h[3];
            var e = h[4];
            var f = h[5];
            var g = h[6];
            var hh = h[7];
            for (var t = 0; t < 64; t++) {
                var s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                var ch = (e & f) ^ (~e & M32 & g);
                var t1 = (hh + s1 + ch + k[t] + w[t]) & M32;
                var s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                var maj = (a & b) ^ (a & c) ^ (b & c);
                var t2 = (s0 + maj) & M32;
                hh = g;
                g = f;
                f = e;
                e = (d + t1) & M32;
                d = c;
                c = b;
                b = a;
                a = (t1 + t2) & M32;
            }
            h[0] = (h[0] + a) & M32;
            h[1] = (h[1] + b) & M32;
            h[2] = (h[2] + c) & M32;
            h[3] = (h[3] + d) & M32;
            h[4] = (h[4] + e) & M32;
            h[5] = (h[5] + f) & M32;
            h[6] = (h[6] + g) & M32;
            h[7] = (h[7] + hh) & M32;
        }

        var out = new [32]b;
        for (var i = 0; i < 8; i++) {
            for (var j = 0; j < 4; j++) {
                out[i * 4 + j] = ((h[i] >> (24 - j * 8)) & 0xFF).toNumber();
            }
        }
        return out;
    }

    // Convert a hex string to a ByteArray. Spaces, ':' and '-' are
    // ignored. Returns null on invalid characters or an odd length.
    function parseHex(s as String) as ByteArray? {
        var chars = s.toCharArray();
        var out = []b;
        var hi = -1;
        for (var i = 0; i < chars.size(); i++) {
            var c = chars[i].toNumber();
            if (c == 32 || c == 58 || c == 45) {
                continue;
            }
            var v = nibble(c);
            if (v < 0) {
                return null;
            }
            if (hi < 0) {
                hi = v;
            } else {
                out = out.add((hi << 4) | v);
                hi = -1;
            }
        }
        if (hi >= 0) {
            return null;
        }
        return out;
    }

    function nibble(c as Number) as Number {
        if (c >= 48 && c <= 57) { return c - 48; }   // 0-9
        if (c >= 65 && c <= 70) { return c - 55; }   // A-F
        if (c >= 97 && c <= 102) { return c - 87; }  // a-f
        return -1;
    }
}
