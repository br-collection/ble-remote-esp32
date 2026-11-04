#include "diag.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "auth.h"
#include "ble_service.h"
#include "config.h"

using namespace proto;

// ----- Events ----------------------------------------------------
struct Event {
    uint32_t time;  // millis()
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  len;
    uint8_t  payload[MAX_PAYLOAD];
};
static constexpr size_t LOG_SIZE = 3;  // fits on the display

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static Event    s_log[LOG_SIZE];
static uint32_t s_logCount  = 0;  // total, newest entry at (s_logCount - 1) % LOG_SIZE
static uint32_t s_okCount   = 0;
static uint32_t s_errCount  = 0;
static bool     s_connected = false;
static char     s_peer[18]  = "";
static uint32_t s_connTime  = 0;
static bool     s_dirty     = true;

// ----- Display -------------------------------------------------------
static U8G2*    s_oled = nullptr;
static int      s_sda = -1;
static int      s_scl = -1;
static uint32_t s_lastDraw = 0;
static uint8_t  s_sent[128 * 64 / 8];  // last image sent
static char     s_bootInfo[24] = "";    // "Boot #3 BROWNOUT"

static const char* resetText(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWER";
        case ESP_RST_EXT:       return "RST-BTN";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:       return "WATCHDOG";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        default:                return "?";
    }
}

// Count boots and remember the reason: reveals unexpected restarts
static void countBoot() {
    Preferences prefs;
    prefs.begin("diag", false);
    uint32_t boots = prefs.getUInt("boots", 0) + 1;
    prefs.putUInt("boots", boots);
    prefs.end();
    esp_reset_reason_t r = esp_reset_reason();
    snprintf(s_bootInfo, sizeof(s_bootInfo), "Boot #%u %s/%d", (unsigned)boots, resetText(r), (int)r);
    Serial.printf("%s (reset reason %d)\n", s_bootInfo, (int)r);
}

static constexpr uint32_t REDRAW_MS    = 1000;  // update age and uptime
static constexpr uint32_t HIGHLIGHT_MS = 2000;  // new command inverted

// Does a display answer on these pins? Returns the I2C address or 0.
static uint8_t probe(int sda, int scl) {
    uint8_t found = 0;
    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(10);
    for (uint8_t addr : {0x3C, 0x3D}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            found = addr;
            break;
        }
    }
    Wire.end();
    return found;
}

// Find the display pins: remembered, then common, then all pairs.
static uint8_t findDisplay(int& sda, int& scl) {
    Preferences prefs;
    prefs.begin("oled", false);
    int cs = prefs.getInt("sda", -1);
    int cc = prefs.getInt("scl", -1);
    uint8_t addr = 0;

    if (cs >= 0 && cc >= 0 && (addr = probe(cs, cc))) {
        sda = cs;
        scl = cc;
    } else {
        static const int8_t common[][2] = {{5, 6}, {8, 9}, {4, 5}, {6, 5}, {9, 8},
                                           {2, 3}, {0, 1}, {20, 21}, {3, 4}};
        static const int8_t pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21};
        for (auto& p : common) {
            if ((addr = probe(p[0], p[1]))) {
                sda = p[0];
                scl = p[1];
                break;
            }
        }
        for (int8_t a : pins) {
            for (int8_t b : pins) {
                if (addr || a == b) continue;
                if ((addr = probe(a, b))) {
                    sda = a;
                    scl = b;
                }
            }
        }
        if (addr) {
            prefs.putInt("sda", sda);
            prefs.putInt("scl", scl);
        }
    }
    prefs.end();
    return addr;
}

bool diagInit() {
    countBoot();
    uint8_t addr = 0;
    if (OLED_SDA >= 0 && OLED_SCL >= 0) {
        s_sda = OLED_SDA;
        s_scl = OLED_SCL;
        addr = probe(s_sda, s_scl);
    } else {
        Serial.println("Searching for OLED ...");
        addr = findDisplay(s_sda, s_scl);
    }
    if (!addr) {
        Serial.println("No OLED found, diagnostics on serial only.");
        s_sda = s_scl = -1;
        return false;
    }
    Serial.printf("OLED at SDA=%d SCL=%d, address 0x%02X\n", s_sda, s_scl, addr);

#if OLED_DRIVER == 1
    s_oled = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, s_scl, s_sda);
#else
    s_oled = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, s_scl, s_sda);
#endif
    s_oled->setI2CAddress(addr << 1);
    s_oled->setBusClock(400000);
    s_oled->begin();
    s_oled->setFont(u8g2_font_6x10_tf);
    s_oled->clearDisplay();               // clear the whole display RAM
    memset(s_sent, 0, sizeof(s_sent));
    return true;
}

bool diagPinUsed(int pin) { return pin >= 0 && (pin == s_sda || pin == s_scl); }

// ----- Reports (from the BLE task or loop()) --------------------------
void diagConnected(const char* addr) {
    portENTER_CRITICAL(&s_mux);
    strncpy(s_peer, addr, sizeof(s_peer) - 1);
    s_peer[sizeof(s_peer) - 1] = '\0';
    s_connTime  = millis();
    s_connected = true;
    s_dirty     = true;
    portEXIT_CRITICAL(&s_mux);
}

void diagDisconnected() {
    portENTER_CRITICAL(&s_mux);
    s_connected = false;
    s_dirty     = true;
    portEXIT_CRITICAL(&s_mux);
}

void diagCommand(uint8_t cmd, const uint8_t* payload, size_t len, uint8_t status) {
    if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;
    portENTER_CRITICAL(&s_mux);
    Event& e = s_log[s_logCount % LOG_SIZE];
    e.time   = millis();
    e.cmd    = cmd;
    e.status = status;
    e.len    = (uint8_t)len;
    if (len > 0) memcpy(e.payload, payload, len);
    s_logCount++;
    if (status == ST_OK) {
        s_okCount++;
    } else {
        s_errCount++;
    }
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
}

// ----- Drawing ------------------------------------------------------
static const char* statusText(uint8_t st) {
    switch (st) {
        case ST_OK:            return "OK";
        case ST_BAD_MAC:       return "MAC!";
        case ST_NONCE_INVALID: return "NONCE";
        case ST_UNKNOWN_CMD:   return "UNKN";
        case ST_BAD_FORMAT:    return "FORMAT";
        case ST_EXEC_FAILED:   return "FAILED";
        case ST_BUSY:          return "BUSY";
    }
    return "?";
}

static void fmtAge(char* buf, size_t n, uint32_t ms) {
    unsigned s = ms / 1000;
    if (s < 60) {
        snprintf(buf, n, "%us", s);
    } else if (s < 3600) {
        snprintf(buf, n, "%um", s / 60);
    } else {
        snprintf(buf, n, "%uh", s / 3600);
    }
}

static void drawRight(U8G2& d, int y, const char* s) {
    d.drawStr(128 - d.getStrWidth(s), y, s);
}

static void draw() {
    Event    log[LOG_SIZE];
    uint32_t count, ok, err, connTime;
    bool     conn;
    char     peer[18];

    portENTER_CRITICAL(&s_mux);
    memcpy(log, s_log, sizeof(log));
    count    = s_logCount;
    ok       = s_okCount;
    err      = s_errCount;
    conn     = s_connected;
    connTime = s_connTime;
    memcpy(peer, s_peer, sizeof(peer));
    s_dirty = false;
    portEXIT_CRITICAL(&s_mux);

    uint32_t now = millis();
    char line[32];
    char age[8];
    U8G2& d = *s_oled;
    d.clearBuffer();

    // Header: name and security
    d.drawStr(0, 8, DEVICE_NAME);
    drawRight(d, 8, authEnabled() ? "HMAC" : "OPEN!");

    // Connection (last 3 bytes of the address)
    if (conn) {
        fmtAge(age, sizeof(age), now - connTime);
        snprintf(line, sizeof(line), "Watch %s", strlen(peer) > 9 ? peer + 9 : peer);
        d.drawStr(0, 19, line);
        drawRight(d, 19, age);
    } else {
        d.drawStr(0, 19, "Waiting for watch");
    }
    d.drawHLine(0, 22, 128);

    // Last commands, newest on top
    size_t shown = count < LOG_SIZE ? count : LOG_SIZE;
    if (shown == 0) {
        d.drawStr(0, 33, "No command yet");
        d.drawStr(0, 43, s_bootInfo);
    }
    for (size_t i = 0; i < shown; i++) {
        const Event& e = log[(count - 1 - i) % LOG_SIZE];
        int y = 33 + 10 * i;

        // "C3 01F4AA+" : command and up to 3 payload bytes
        int p = snprintf(line, sizeof(line), "C%u ", e.cmd);
        for (size_t k = 0; k < e.len && k < 3; k++) {
            p += snprintf(line + p, sizeof(line) - p, "%02X", e.payload[k]);
        }
        if (e.len > 3) snprintf(line + p, sizeof(line) - p, "+");
        fmtAge(age, sizeof(age), now - e.time);

        bool highlight = (i == 0) && (now - e.time) < HIGHLIGHT_MS;
        if (highlight) {
            d.drawBox(0, y - 8, 128, 10);
            d.setDrawColor(0);
        }
        d.drawStr(0, y, line);
        d.drawStr(66, y, statusText(e.status));
        drawRight(d, y, age);
        d.setDrawColor(1);
    }

    // Footer: counters and uptime
    unsigned up = now / 60000;
    snprintf(line, sizeof(line), "OK %u Err %u", (unsigned)ok, (unsigned)err);
    d.drawStr(0, 63, line);
    snprintf(line, sizeof(line), "%uh%02u", up / 60, up % 60);
    drawRight(d, 63, line);

    // Only send if the image changed (less I2C, no flicker)
    const uint8_t* buf = d.getBufferPtr();
    if (memcmp(buf, s_sent, sizeof(s_sent)) != 0) {
        memcpy(s_sent, buf, sizeof(s_sent));
        d.sendBuffer();
    }
}

void diagShowText(const char* line1, const char* line2) {
    Serial.printf("%s %s\n", line1, line2);
    if (s_oled == nullptr) return;
    U8G2& d = *s_oled;
    d.clearBuffer();
    d.drawStr(0, 10, line1);
    d.setFont(u8g2_font_logisoso24_tf);
    d.drawStr(0, 50, line2);
    d.setFont(u8g2_font_6x10_tf);
    d.sendBuffer();
    memset(s_sent, 0, sizeof(s_sent));  // send the diagnostics again afterwards
}

void diagLoop() {
    if (s_oled == nullptr) return;
    uint32_t now = millis();
    bool dirty;
    portENTER_CRITICAL(&s_mux);
    dirty = s_dirty;
    portEXIT_CRITICAL(&s_mux);
    if (dirty || now - s_lastDraw >= REDRAW_MS) {
        s_lastDraw = now;
        draw();
    }
}
