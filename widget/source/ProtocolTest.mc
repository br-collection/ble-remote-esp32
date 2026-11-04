// Tests for HMAC-SHA256 (test build only, simulator: monkeydo ... /t)
import Toybox.Lang;
import Toybox.StringUtil;
import Toybox.Test;

(:test)
function testSha256Abc(logger as Logger) as Boolean {
    var h = Protocol.sha256(StringUtil.convertEncodedString("abc",
        {:fromRepresentation => StringUtil.REPRESENTATION_STRING_PLAIN_TEXT,
         :toRepresentation => StringUtil.REPRESENTATION_BYTE_ARRAY}) as ByteArray);
    return tHex(h).equals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// RFC 4231, test case 2
(:test)
function testHmacRfc4231(logger as Logger) as Boolean {
    var key = tBytes("Jefe");
    var data = tBytes("what do ya want for nothing?");
    var soft = tHex(Protocol.hmacSoft(key, data));
    logger.debug("soft " + soft);
    return soft.equals("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

// Software and watch function give the same result (32-byte key, data longer than a block)
(:test)
function testHmacMatchesNative(logger as Logger) as Boolean {
    var key = new [32]b;
    var data = new [70]b;
    for (var i = 0; i < 70; i++) {
        if (i < 32) { key[i] = (i * 7 + 3) & 0xFF; }
        data[i] = (i * 13 + 1) & 0xFF;
    }
    return tHex(Protocol.hmacSoft(key, data)).equals(tHex(Protocol.hmacSha256(key, data)));
}

function tBytes(s as String) as ByteArray {
    return StringUtil.convertEncodedString(s,
        {:fromRepresentation => StringUtil.REPRESENTATION_STRING_PLAIN_TEXT,
         :toRepresentation => StringUtil.REPRESENTATION_BYTE_ARRAY}) as ByteArray;
}

function tHex(b as ByteArray) as String {
    return StringUtil.convertEncodedString(b,
        {:fromRepresentation => StringUtil.REPRESENTATION_BYTE_ARRAY,
         :toRepresentation => StringUtil.REPRESENTATION_STRING_HEX}) as String;
}
