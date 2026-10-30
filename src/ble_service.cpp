#include "ble_service.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#else
#include <esp_system.h>
#endif

#include "auth.h"
#include "commands.h"
#include "config.h"
#include "diag.h"

using namespace proto;

// ----- GATT-Objekte --------------------------------------------------
static NimBLEServer*         s_server    = nullptr;
static NimBLECharacteristic* s_nonceChr  = nullptr;
static NimBLECharacteristic* s_statusChr = nullptr;

// ----- Sitzung (nur eine Verbindung gleichzeitig) --------------------
static volatile bool     s_connected     = false;
static volatile bool     s_disconnecting = false;
static volatile uint16_t s_connHandle    = 0;
static volatile uint32_t s_connectTime   = 0;
static volatile uint8_t  s_failCount     = 0;
static uint8_t           s_paramRequests = 0;  // Anforderungen kurzes Intervall

// Nonce wird nur im BLE-Host-Task verwendet
static uint8_t  s_nonce[NONCE_LEN];
static bool     s_nonceValid = false;
static uint32_t s_nonceTime  = 0;

// ----- Übergabe BLE-Task -> loop() -----------------------------------
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool    s_pending = false;
static uint8_t s_pendingCmd = 0;
static uint8_t s_pendingPayload[MAX_PAYLOAD];
static size_t  s_pendingLen = 0;

// ---------------------------------------------------------------------
static void setStatus(uint8_t st) { s_statusChr->setValue(&st, 1); }

static void newNonce() {
    esp_fill_random(s_nonce, NONCE_LEN);
    s_nonceValid = true;
    s_nonceTime  = millis();
    s_nonceChr->setValue(s_nonce, NONCE_LEN);
}

static void clearNonce() {
    uint8_t zero[NONCE_LEN] = {0};
    s_nonceValid = false;
    s_nonceChr->setValue(zero, NONCE_LEN);
}

// Prüft einen empfangenen Frame. Bei Erfolg wird der Befehl an loop()
// übergeben und ST_BUSY zurückgegeben.
static uint8_t handleFrame(const uint8_t* d, size_t n) {
    bool busy;
    portENTER_CRITICAL(&s_mux);
    busy = s_pending;
    portEXIT_CRITICAL(&s_mux);
    if (busy) return ST_BUSY;

    if (n < HEADER_LEN || n > MAX_FRAME) return ST_BAD_FORMAT;
    if (d[0] != VERSION) return ST_BAD_FORMAT;
    uint8_t cmd = d[1];
    size_t payloadLen;

    if (authEnabled()) {
        if (n < HEADER_LEN + MAC_LEN) return ST_BAD_FORMAT;
        payloadLen = n - HEADER_LEN - MAC_LEN;

        if (!s_nonceValid || (millis() - s_nonceTime) > NONCE_TTL_MS) {
            newNonce();
            return ST_NONCE_INVALID;
        }
        bool ok = authVerify(s_nonce, NONCE_LEN, d, HEADER_LEN + payloadLen,
                             d + HEADER_LEN + payloadLen, MAC_LEN);
        newNonce();  // jede Nonce gilt nur für einen Versuch
        if (!ok) {
            s_failCount = s_failCount + 1;
            return ST_BAD_MAC;
        }
    } else {
        payloadLen = n - HEADER_LEN;
    }

    if (payloadLen > MAX_PAYLOAD) return ST_BAD_FORMAT;
    if (!commandExists(cmd)) return ST_UNKNOWN_CMD;

    portENTER_CRITICAL(&s_mux);
    s_pendingCmd = cmd;
    memcpy(s_pendingPayload, d + HEADER_LEN, payloadLen);
    s_pendingLen = payloadLen;
    s_pending = true;
    portEXIT_CRITICAL(&s_mux);
    return ST_BUSY;
}

// ----- Callbacks -----------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
        if (s_connected) {  // nur eine Verbindung zulassen
            srv->disconnect(info.getConnHandle());
            return;
        }
        s_connHandle    = info.getConnHandle();
        s_connectTime   = millis();
        s_failCount     = 0;
        s_disconnecting = false;
        s_connected     = true;
        newNonce();
        setStatus(ST_NO_COMMAND);
        std::string addr = info.getAddress().toString();
        diagConnected(addr.c_str());
        Serial.printf("Verbunden: %s, Intervall %.1f ms\n", addr.c_str(),
                      info.getConnInterval() * 1.25f);
        // Kurzes Verbindungsintervall anfordern: jeder Lese-/Schreibvorgang
        // wartet auf das nächste Intervall. Die Uhr darf ablehnen.
        s_paramRequests = 0;
        requestFastInterval(info.getConnHandle());
    }

    // Die FR945 stellt kurz nach dem Verbinden auf 1000 ms um: dann erneut
    // anfordern, begrenzt, damit es kein Hin und Her ohne Ende gibt.
    void onConnParamsUpdate(NimBLEConnInfo& info) override {
        Serial.printf("Intervall jetzt %.1f ms\n", info.getConnInterval() * 1.25f);
        if (info.getConnInterval() > CONN_INTERVAL_MAX && s_paramRequests < CONN_PARAM_RETRIES) {
            requestFastInterval(info.getConnHandle());
        }
    }

    void requestFastInterval(uint16_t handle) {
        s_paramRequests++;
        s_server->updateConnParams(handle, CONN_INTERVAL_MIN, CONN_INTERVAL_MAX, 0,
                                   CONN_SUPERVISION_TIMEOUT);
    }

    void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int reason) override {
        if (info.getConnHandle() != s_connHandle) return;
        s_connected = false;
        clearNonce();
        diagDisconnected();
        Serial.printf("Getrennt (Grund %d)\n", reason);
    }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& info) override {
        NimBLEAttValue v = chr->getValue();
        uint8_t st = handleFrame(v.data(), v.length());
        setStatus(st);
        if (st != ST_BUSY) {
            // abgelehnt: Befehl anzeigen, Payload nicht (Länge mit/ohne HMAC unklar)
            diagCommand(v.length() >= HEADER_LEN ? v.data()[1] : 0, nullptr, 0, st);
            Serial.printf("Befehl abgelehnt, Status 0x%02X\n", st);
        }
    }
};

// ----- Öffentliche Funktionen ----------------------------------------
void bleInit() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(BLE_TX_DBM);
    Serial.printf("Sendeleistung %d dBm\n", NimBLEDevice::getPower());
    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(new ServerCallbacks());
    s_server->advertiseOnDisconnect(true);

    NimBLEService* svc = s_server->createService(SERVICE_UUID);
    s_nonceChr = svc->createCharacteristic(NONCE_UUID, NIMBLE_PROPERTY::READ);
    NimBLECharacteristic* cmdChr =
        svc->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    s_statusChr = svc->createCharacteristic(STATUS_UUID, NIMBLE_PROPERTY::READ);
    cmdChr->setCallbacks(new CommandCallbacks());  // Service startet mit dem Server (NimBLE 2.x)

    clearNonce();
    setStatus(ST_NO_COMMAND);

    // Service-UUID im Advertising, Name in der Scan-Response
    // (beides zusammen passt nicht in 31 Byte)
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));

    NimBLEAdvertisementData scanData;
    scanData.setName(DEVICE_NAME);

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementData(advData);
    adv->setScanResponseData(scanData);
    adv->enableScanResponse(true);
    adv->setMinInterval(ADV_INTERVAL_MIN);
    adv->setMaxInterval(ADV_INTERVAL_MAX);
    adv->start();
}

void bleLoop() {
    // Anstehenden Befehl ausführen
    bool    have = false;
    uint8_t cmd = 0;
    uint8_t payload[MAX_PAYLOAD];
    size_t  len = 0;

    portENTER_CRITICAL(&s_mux);
    if (s_pending) {
        have = true;
        cmd  = s_pendingCmd;
        len  = s_pendingLen;
        memcpy(payload, s_pendingPayload, len);
    }
    portEXIT_CRITICAL(&s_mux);

    if (have) {
        Serial.printf("Fuehre Befehl %u aus\n", cmd);
        bool ok = commandExecute(cmd, payload, len);
        setStatus(ok ? ST_OK : ST_EXEC_FAILED);
        diagCommand(cmd, payload, len, ok ? ST_OK : ST_EXEC_FAILED);
        portENTER_CRITICAL(&s_mux);
        s_pending = false;
        portEXIT_CRITICAL(&s_mux);
    }

    // Verbindung trennen bei Zeitüberschreitung oder zu vielen Fehlversuchen
    if (s_connected && !s_disconnecting) {
        bool timeout  = (millis() - s_connectTime) > SESSION_TIMEOUT_MS;
        bool tooMany  = s_failCount >= MAX_FAILED_ATTEMPTS;
        if (timeout || tooMany) {
            s_disconnecting = true;
            Serial.println(timeout ? "Sitzung abgelaufen, trenne"
                                   : "Zu viele Fehlversuche, trenne");
            s_server->disconnect(s_connHandle);
        }
    }
}
