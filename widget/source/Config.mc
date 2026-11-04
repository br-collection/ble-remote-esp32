// Reads and checks the settings from the Garmin Connect app
import Toybox.Application;
import Toybox.BluetoothLowEnergy;
import Toybox.Lang;

class Config {
    var key as ByteArray? = null;           // null = no security
    var nameFilter as String = "";
    var scanTimeoutMs as Number = 10000;
    var enabled as Array<Boolean> = [false, false, false, false];
    var names as Array<String> = ["", "", "", ""];
    var payloads as Array<ByteArray> = [[]b, []b, []b, []b];

    // BluetoothLowEnergy.Uuid
    var uuidService = null;
    var uuidNonce = null;
    var uuidCommand = null;
    var uuidStatus = null;
    var uuidSignature as String = "";       // to detect UUID changes

    function initialize() {}

    // Returns null if everything is valid, otherwise the resource with the
    // name of the invalid field
    function load() {
        var keyStr = getStr("key");
        if (keyStr.length() > 0) {
            var k = Protocol.parseHex(keyStr);
            if (k == null || k.size() < 16 || k.size() > 64) {
                return Rez.Strings.SetKey;
            }
            key = k;
        }

        nameFilter = getStr("nameFilter");
        scanTimeoutMs = clamp(getNum("scanTimeout", 10), 3, 30) * 1000;

        var payloadTitles = [Rez.Strings.SetCmd1Payload, Rez.Strings.SetCmd2Payload,
                             Rez.Strings.SetCmd3Payload, Rez.Strings.SetCmd4Payload];
        for (var i = 0; i < 4; i++) {
            var n = (i + 1).toString();
            enabled[i] = getBool("cmd" + n + "On", true);
            var nm = getStr("cmd" + n + "Name");
            names[i] = (nm.length() > 0) ? nm : ("#" + n);
            var p = Protocol.parseHex(getStr("cmd" + n + "Payload"));
            if (p == null || p.size() > Protocol.MAX_PAYLOAD) {
                return payloadTitles[i];
            }
            payloads[i] = p;
        }

        var keys = ["uuidService", "uuidNonce", "uuidCommand", "uuidStatus"];
        var titles = [Rez.Strings.SetUuidService, Rez.Strings.SetUuidNonce,
                      Rez.Strings.SetUuidCommand, Rez.Strings.SetUuidStatus];
        var uuids = new [4];
        uuidSignature = "";
        for (var i = 0; i < 4; i++) {
            var s = getStr(keys[i]);
            try {
                uuids[i] = BluetoothLowEnergy.stringToUuid(s);
            } catch (e) {
                return titles[i];
            }
            uuidSignature += s.toLower() + ";";
        }
        uuidService = uuids[0];
        uuidNonce = uuids[1];
        uuidCommand = uuids[2];
        uuidStatus = uuids[3];
        return null;
    }

    function activeCount() as Number {
        var c = 0;
        for (var i = 0; i < 4; i++) {
            if (enabled[i]) { c++; }
        }
        return c;
    }

    private function getStr(k as String) as String {
        var v = Application.Properties.getValue(k);
        return (v == null) ? "" : v.toString();
    }

    private function getNum(k as String, def as Number) as Number {
        var v = Application.Properties.getValue(k);
        return (v instanceof Number) ? v as Number : def;
    }

    private function getBool(k as String, def as Boolean) as Boolean {
        var v = Application.Properties.getValue(k);
        return (v instanceof Boolean) ? v as Boolean : def;
    }

    private function clamp(v as Number, lo as Number, hi as Number) as Number {
        if (v < lo) { return lo; }
        if (v > hi) { return hi; }
        return v;
    }
}
