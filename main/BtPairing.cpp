#include "BtPairing.h"
#include <arduino_platform.h>

namespace BtPairing {
namespace {

constexpr int MAX_WHITELIST = 16;
constexpr int MAX_FOUND = 10;

struct FoundDevice {
    uint8_t addr[6];
    char name[32];
    int8_t rssiDbm;      // 0 = unbekannt (eingehende Verbindung ohne Suche)
    bool allowed;
    bool ready;          // hat eine Verbindung aufgebaut (nicht nur gefunden)
    uint32_t lastSeenMs;
};

// Der Filter läuft im BTstack-Thread, Whitelist und Status kommen aus Webserver und Loop.
// Kurze kritische Abschnitte ohne String-Operationen.
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
bool wlEnabled = false;
uint8_t wlAddrs[MAX_WHITELIST][6];
int wlCount = 0;
FoundDevice found[MAX_FOUND];
int foundCount = 0;
bool searching = false;
uint32_t searchEndMs = 0;

bool parseMac(const String& s, uint8_t out[6]) {
    unsigned v[6];
    if (sscanf(s.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
    return true;
}

bool inWhitelistLocked(const uint8_t addr[6]) {
    for (int i = 0; i < wlCount; i++) {
        if (memcmp(wlAddrs[i], addr, 6) == 0) return true;
    }
    return false;
}

void recordLocked(const uint8_t addr[6], const char* name, uint8_t rssi, bool allowed, bool ready, uint32_t now) {
    int idx = -1;
    for (int i = 0; i < foundCount; i++) {
        if (memcmp(found[i].addr, addr, 6) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        if (foundCount < MAX_FOUND) {
            idx = foundCount++;
        } else {
            // Liste voll: ältesten Eintrag ersetzen
            idx = 0;
            for (int i = 1; i < MAX_FOUND; i++) {
                if ((int32_t)(found[i].lastSeenMs - found[idx].lastSeenMs) < 0) idx = i;
            }
        }
        memcpy(found[idx].addr, addr, 6);
        found[idx].name[0] = '\0';
        found[idx].rssiDbm = 0;
        found[idx].ready = false;
    }
    if (name && name[0]) {
        strncpy(found[idx].name, name, sizeof(found[idx].name) - 1);
        found[idx].name[sizeof(found[idx].name) - 1] = '\0';
    }
    // BTstack liefert den RSSI als vorzeichenlosen Wert, gemeint ist dBm (int8)
    if (rssi) found[idx].rssiDbm = (int8_t)rssi;
    found[idx].allowed = allowed;
    if (ready) found[idx].ready = true;
    found[idx].lastSeenMs = now;
}

bool deviceFilter(const uint8_t addr[6], const char* name, uint8_t rssi, bool ready) {
    uint32_t now = millis();
    bool allowed;
    portENTER_CRITICAL(&mux);
    allowed = !wlEnabled || inWhitelistLocked(addr);
    recordLocked(addr, name, rssi, allowed, ready, now);
    portEXIT_CRITICAL(&mux);
    return allowed;
}

void expireLocked(uint32_t now) {
    if (searching && (int32_t)(now - searchEndMs) >= 0) searching = false;
}

}  // namespace

void begin() {
    arduino_set_device_filter(&deviceFilter);
}

void applyWhitelist(bool enabled, const std::vector<String>& macs) {
    uint8_t addrs[MAX_WHITELIST][6];
    int count = 0;
    for (const auto& mac : macs) {
        if (count >= MAX_WHITELIST) {
            Serial.printf("BT-Whitelist: mehr als %d Einträge, Rest wird ignoriert\n", MAX_WHITELIST);
            break;
        }
        if (parseMac(mac, addrs[count])) {
            count++;
        } else {
            Serial.printf("BT-Whitelist: ungültige MAC '%s' ignoriert\n", mac.c_str());
        }
    }
    portENTER_CRITICAL(&mux);
    wlEnabled = enabled;
    memcpy(wlAddrs, addrs, sizeof(addrs));
    wlCount = count;
    portEXIT_CRITICAL(&mux);
    Serial.printf("BT-Whitelist: %s, %d Einträge\n", enabled ? "aktiv" : "aus", count);
}

void startSearch(uint32_t durationMs) {
    uint32_t now = millis();
    portENTER_CRITICAL(&mux);
    searching = true;
    searchEndMs = now + durationMs;
    foundCount = 0;
    portEXIT_CRITICAL(&mux);
}

void stopSearch() {
    portENTER_CRITICAL(&mux);
    searching = false;
    portEXIT_CRITICAL(&mux);
}

bool isSearching() {
    uint32_t now = millis();
    portENTER_CRITICAL(&mux);
    expireLocked(now);
    bool s = searching;
    portEXIT_CRITICAL(&mux);
    return s;
}

uint32_t remainingMs() {
    uint32_t now = millis();
    portENTER_CRITICAL(&mux);
    expireLocked(now);
    uint32_t r = searching ? (searchEndMs - now) : 0;
    portEXIT_CRITICAL(&mux);
    return r;
}

void fillStatus(JsonDocument& doc) {
    FoundDevice snapshot[MAX_FOUND];
    int count;
    bool active;
    bool wl;
    uint32_t remaining;
    uint32_t now = millis();
    portENTER_CRITICAL(&mux);
    expireLocked(now);
    active = searching;
    remaining = searching ? (searchEndMs - now) : 0;
    wl = wlEnabled;
    count = foundCount;
    memcpy(snapshot, found, sizeof(FoundDevice) * count);
    portEXIT_CRITICAL(&mux);

    doc["active"] = active;
    doc["remaining_ms"] = remaining;
    doc["whitelist_enabled"] = wl;
    JsonArray arr = doc["devices"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["mac"] = formatMac(snapshot[i].addr);
        o["name"] = snapshot[i].name;
        if (snapshot[i].rssiDbm != 0) o["rssi"] = snapshot[i].rssiDbm;
        o["allowed"] = snapshot[i].allowed;
        o["ready"] = snapshot[i].ready;
    }
}

String formatMac(const uint8_t addr[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return String(buf);
}

}  // namespace BtPairing
