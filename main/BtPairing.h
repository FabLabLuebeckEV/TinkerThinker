#ifndef BT_PAIRING_H
#define BT_PAIRING_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Controller-Suche ("Such-Fenster") und eigene Bluetooth-Whitelist.
//
// Bekannte (gekoppelte) Controller verbinden sich jederzeit von selbst, weil das Board immer
// verbindbar ist (Page Scan). Die teure Gerätesuche (Inquiry) läuft nur in einem zeitlich
// begrenzten Such-Fenster: nach dem Start, per MODE-Taste oder per Browser. So bleibt das
// WLAN die meiste Zeit ungestört.
//
// Die Whitelist wird hier statt in der Bluepad32-Allowlist geprüft. Dadurch sieht die
// BT-Suche im Browser auch Controller, die (noch) nicht erlaubt sind, und es gibt keine
// NVS-Schreibzugriffe bei jedem Start.
namespace BtPairing {

// Hook in der Bluepad32-Arduino-Brücke registrieren. Einmal in setup() aufrufen.
void begin();

// Whitelist übernehmen (aus ConfigManager). Thread-sicher.
void applyWhitelist(bool enabled, const std::vector<String>& macs);

// Such-Fenster starten (leert die Liste gefundener Geräte) bzw. beenden. Thread-sicher.
void startSearch(uint32_t durationMs);
void stopSearch();
bool isSearching();
uint32_t remainingMs();

// Status für GET /bt/scan: {active, remaining_ms, whitelist_enabled, devices:[...]}
void fillStatus(JsonDocument& doc);

// MAC im üblichen Format "AA:BB:CC:DD:EE:FF" (Bytes in BTstack-Reihenfolge [0]..[5]).
String formatMac(const uint8_t addr[6]);

}  // namespace BtPairing

#endif
