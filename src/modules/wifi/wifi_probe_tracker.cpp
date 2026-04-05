/**
 * @file wifi_probe_tracker.cpp
 * @brief WiFi probe request tracker and signal intelligence dashboard
 * @details Passively monitors WiFi probe requests to track nearby devices
 */

#if !defined(LITE_VERSION)
#include "wifi_probe_tracker.h"
#include "core/display.h"
#include "core/led_feedback.h"
#include "core/mykeyboard.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <globals.h>
#include <map>
#include <vector>

// ── Tracked Device ─────────────────────────────────────────────────────────────

struct TrackedDevice {
    uint8_t mac[6];
    String lastSSID;          // Last probed SSID
    std::vector<String> ssids; // All probed SSIDs (max 10)
    int8_t rssi;
    int8_t minRssi;
    int8_t maxRssi;
    unsigned long firstSeen;
    unsigned long lastSeen;
    int probeCount;
    uint8_t lastChannel;
    bool isRandomMAC;         // Randomized MAC detection
};

static std::vector<TrackedDevice> trackedDevices;
static constexpr int MAX_TRACKED = 64;
static volatile int newProbeCount = 0;

// ── MAC to String ──────────────────────────────────────────────────────────────

static String macToStr(const uint8_t *mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

// ── Check if MAC is locally administered (randomized) ──────────────────────────

static bool isRandomizedMAC(const uint8_t *mac) {
    return (mac[0] & 0x02) != 0; // Bit 1 of first octet = locally administered
}

// ── Vendor lookup by OUI ───────────────────────────────────────────────────────

struct ProbeOUI {
    uint8_t prefix[3];
    const char *vendor;
};

static const ProbeOUI probeVendors[] = {
    {{0xAC, 0xDE, 0x48}, "Apple"},
    {{0x3C, 0xE0, 0x72}, "Apple"},
    {{0xF0, 0x18, 0x98}, "Apple"},
    {{0x30, 0x35, 0xAD}, "Apple"},
    {{0x7C, 0x64, 0x56}, "Samsung"},
    {{0x00, 0x17, 0xC9}, "Samsung"},
    {{0x58, 0xCB, 0x52}, "Google"},
    {{0x20, 0xDF, 0xB9}, "Google"},
    {{0x98, 0x09, 0xCF}, "Microsoft"},
    {{0xDC, 0xA6, 0x32}, "RPi"},
    {{0xB8, 0x27, 0xEB}, "RPi"},
    {{0x00, 0x1A, 0x11}, "Google"},
    {{0xFC, 0xE9, 0x98}, "Apple"},
};
static constexpr int PROBE_VENDOR_COUNT = sizeof(probeVendors) / sizeof(probeVendors[0]);

static String vendorFromMAC(const uint8_t *mac) {
    if (isRandomizedMAC(mac)) return "Random";
    for (int i = 0; i < PROBE_VENDOR_COUNT; i++) {
        if (mac[0] == probeVendors[i].prefix[0] &&
            mac[1] == probeVendors[i].prefix[1] &&
            mac[2] == probeVendors[i].prefix[2]) {
            return probeVendors[i].vendor;
        }
    }
    return "?";
}

// ── Promiscuous Mode Callback ──────────────────────────────────────────────────

static void IRAM_ATTR probeSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    if (len < 24) return;

    // Check frame type: Management, subtype Probe Request (0x04)
    uint8_t frameType = frame[0];
    if ((frameType & 0xFC) != 0x40) return; // Not a probe request

    // Extract source MAC (bytes 10-15)
    const uint8_t *srcMAC = &frame[10];

    // Extract SSID from tagged parameters (starts at byte 24)
    String ssid = "";
    if (len > 26) {
        int pos = 24;
        while (pos < len - 2) {
            uint8_t tagNum = frame[pos];
            uint8_t tagLen = frame[pos + 1];
            if (pos + 2 + tagLen > len) break;
            if (tagNum == 0 && tagLen > 0 && tagLen < 33) {
                // SSID tag
                char ssidBuf[33] = {};
                memcpy(ssidBuf, &frame[pos + 2], tagLen);
                ssid = ssidBuf;
            }
            pos += 2 + tagLen;
        }
    }

    // Find or create device entry
    bool found = false;
    for (auto &dev : trackedDevices) {
        if (memcmp(dev.mac, srcMAC, 6) == 0) {
            dev.rssi = pkt->rx_ctrl.rssi;
            if (pkt->rx_ctrl.rssi < dev.minRssi) dev.minRssi = pkt->rx_ctrl.rssi;
            if (pkt->rx_ctrl.rssi > dev.maxRssi) dev.maxRssi = pkt->rx_ctrl.rssi;
            dev.lastSeen = millis();
            dev.probeCount++;
            dev.lastChannel = pkt->rx_ctrl.channel;
            if (ssid.length() > 0) {
                dev.lastSSID = ssid;
                // Add to SSID list if not already there
                bool ssidFound = false;
                for (auto &s : dev.ssids) {
                    if (s == ssid) { ssidFound = true; break; }
                }
                if (!ssidFound && dev.ssids.size() < 10) dev.ssids.push_back(ssid);
            }
            found = true;
            break;
        }
    }

    if (!found && (int)trackedDevices.size() < MAX_TRACKED) {
        TrackedDevice dev = {};
        memcpy(dev.mac, srcMAC, 6);
        dev.rssi = pkt->rx_ctrl.rssi;
        dev.minRssi = pkt->rx_ctrl.rssi;
        dev.maxRssi = pkt->rx_ctrl.rssi;
        dev.firstSeen = millis();
        dev.lastSeen = millis();
        dev.probeCount = 1;
        dev.lastChannel = pkt->rx_ctrl.channel;
        dev.isRandomMAC = isRandomizedMAC(srcMAC);
        if (ssid.length() > 0) {
            dev.lastSSID = ssid;
            dev.ssids.push_back(ssid);
        }
        trackedDevices.push_back(dev);
    }

    newProbeCount++;
}

// ── WiFi Probe Tracker ─────────────────────────────────────────────────────────

void wifiProbeTracker() {
    ledFeedbackSetMode(LED_FB_WIFI_ATTACK);
    trackedDevices.clear();
    newProbeCount = 0;

    // Start promiscuous mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(probeSnifferCallback);

    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("PROBE TRACKER", tft.width() / 2, 5, 1);

    int currentChannel = 1;
    unsigned long lastHop = 0;
    int displayOffset = 0;

    while (1) {
        if (check(EscPress)) break;

        // Channel hopping every 200ms
        if (millis() - lastHop > 200) {
            currentChannel = (currentChannel % 13) + 1;
            esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
            lastHop = millis();
        }

        // Scroll with encoder
        if (check(NextPress) && displayOffset < (int)trackedDevices.size() - 4) displayOffset++;
        if (check(PrevPress) && displayOffset > 0) displayOffset--;

        // Redraw
        tft.fillRect(0, 20, tft.width(), tft.height() - 20, bruceConfig.bgColor);

        // Header
        tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
        tft.drawString("Devices: " + String(trackedDevices.size()) + " Probes: " + String(newProbeCount) + " Ch:" + String(currentChannel), 5, 22, 1);

        // Sort by last seen (most recent first)
        // Simple display without sorting to avoid complexity in ISR context
        int y = 38;
        int shown = 0;
        for (int i = displayOffset; i < (int)trackedDevices.size() && shown < 7; i++) {
            auto &dev = trackedDevices[i];
            unsigned long age = (millis() - dev.lastSeen) / 1000;

            // Color code by recency
            if (age < 5) tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            else if (age < 30) tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            else tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);

            // Line 1: MAC + vendor + RSSI
            String vendor = vendorFromMAC(dev.mac);
            String line1 = macToStr(dev.mac).substring(9) + " " + vendor + " " + String(dev.rssi) + "dB";
            tft.drawString(line1, 5, y, 1);
            y += 12;

            // Line 2: SSID + probe count
            String ssidDisplay = dev.lastSSID.length() > 0 ? dev.lastSSID.substring(0, 14) : "(broadcast)";
            tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
            tft.drawString(" " + ssidDisplay + " x" + String(dev.probeCount) + " " + String(age) + "s", 5, y, 1);
            y += 14;

            shown++;
        }

        delay(100);
    }

    // Cleanup
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    // Show summary if devices found
    if (!trackedDevices.empty()) {
        options.clear();
        for (size_t i = 0; i < trackedDevices.size(); i++) {
            auto &dev = trackedDevices[i];
            String label = macToStr(dev.mac).substring(9) + " " + vendorFromMAC(dev.mac) + " x" + String(dev.probeCount);
            size_t idx = i;
            options.push_back({label, [idx]() {
                auto &d = trackedDevices[idx];
                tft.fillScreen(bruceConfig.bgColor);
                tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
                tft.drawCentreString("DEVICE DETAIL", tft.width() / 2, 5, 1);
                int y = 25;
                tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
                tft.drawString("MAC: " + macToStr(d.mac), 5, y, 1); y += 15;
                tft.drawString("Vendor: " + vendorFromMAC(d.mac), 5, y, 1); y += 15;
                tft.drawString("RSSI: " + String(d.rssi) + " (min:" + String(d.minRssi) + " max:" + String(d.maxRssi) + ")", 5, y, 1); y += 15;
                tft.drawString("Probes: " + String(d.probeCount), 5, y, 1); y += 15;
                tft.drawString("Random MAC: " + String(d.isRandomMAC ? "Yes" : "No"), 5, y, 1); y += 15;

                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                tft.drawString("Probed SSIDs:", 5, y, 1); y += 15;
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                for (auto &ssid : d.ssids) {
                    tft.drawString(" " + ssid, 10, y, 1);
                    y += 12;
                    if (y > tft.height() - 15) break;
                }
                while (!check(EscPress)) delay(50);
            }});
        }
        addOptionToMainMenu();
        loopOptions(options);
    }

    trackedDevices.clear();
    ledFeedbackRestore();
}

// ── WiFi Client Tracker ────────────────────────────────────────────────────────

void wifiClientTracker() {
    ledFeedbackSetMode(LED_FB_WIFI_ATTACK);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("CLIENT TRACKER", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Scanning networks...", tft.width() / 2, 25, 1);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int nets = WiFi.scanNetworks(false, true);

    if (nets <= 0) {
        displayError("No networks found!", true);
        ledFeedbackRestore();
        return;
    }

    // Show networks and client counts
    options.clear();
    for (int i = 0; i < nets && i < 20; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "(hidden)";
        int ch = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);
        String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "ENC";

        String label = ssid.substring(0, 12) + " ch" + String(ch) + " " + String(rssi) + "dB " + enc;
        int channel = ch;
        String bssidStr = WiFi.BSSIDstr(i);

        options.push_back({label, [channel, bssidStr, ssid]() {
            // Monitor this specific channel for data frames
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawCentreString("Monitoring: " + ssid, tft.width() / 2, 5, 1);
            tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
            tft.drawCentreString("BSSID: " + bssidStr, tft.width() / 2, 25, 1);
            tft.drawCentreString("Channel: " + String(channel), tft.width() / 2, 40, 1);
            tft.drawCentreString("Monitoring traffic...", tft.width() / 2, 60, 1);
            tft.drawCentreString("[ESC] to stop", tft.width() / 2, tft.height() - 15, 1);

            // Set to that channel and monitor
            esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            while (!check(EscPress)) delay(100);
        }});
    }

    WiFi.scanDelete();
    addOptionToMainMenu();
    loopOptions(options);
    ledFeedbackRestore();
}

// ── Signal Intelligence Dashboard ──────────────────────────────────────────────

void signalIntelDashboard() {
    ledFeedbackSetMode(LED_FB_RF_SCAN);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("SIGINT DASHBOARD", tft.width() / 2, 5, 1);

    // Start WiFi promiscuous for probe counting
    trackedDevices.clear();
    newProbeCount = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(probeSnifferCallback);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);

    int wifiChannel = 1;
    unsigned long lastUpdate = 0;
    int totalProbes = 0;

    while (1) {
        if (check(EscPress)) break;

        // Hop WiFi channels
        wifiChannel = (wifiChannel % 13) + 1;
        esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);

        if (millis() - lastUpdate > 500) {
            lastUpdate = millis();
            totalProbes = newProbeCount;

            tft.fillRect(0, 22, tft.width(), tft.height() - 22, bruceConfig.bgColor);
            int y = 25;

            // WiFi section
            tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
            tft.drawString("WiFi", 5, y, 1);
            tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
            int uniqueDevs = trackedDevices.size();
            int randomMACs = 0;
            for (auto &d : trackedDevices) if (d.isRandomMAC) randomMACs++;
            tft.drawString(" Dev:" + String(uniqueDevs) + " Probes:" + String(totalProbes) + " Rand:" + String(randomMACs), 5, y + 12, 1);
            y += 30;

            // Show top 3 most active devices
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            int topShown = 0;
            for (auto &d : trackedDevices) {
                if (topShown >= 3) break;
                unsigned long age = (millis() - d.lastSeen) / 1000;
                if (age > 10) continue;
                String vendor = vendorFromMAC(d.mac);
                tft.drawString(" " + vendor + " " + String(d.rssi) + "dB x" + String(d.probeCount), 10, y, 1);
                y += 12;
                topShown++;
            }
            if (topShown == 0) {
                tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
                tft.drawString(" (no recent activity)", 10, y, 1);
                y += 12;
            }

            y += 5;

            // BLE section (just status)
            tft.setTextColor(TFT_BLUE, bruceConfig.bgColor);
            tft.drawString("BLE", 5, y, 1);
            tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
            tft.drawString(" (use BLE Recon for scan)", 5, y + 12, 1);
            y += 30;

            // RF section (just status)
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.drawString("Sub-GHz (CC1101)", 5, y, 1);
            tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
            tft.drawString(" Freq: " + String(bruceConfig.rfFreq, 2) + " MHz", 5, y + 12, 1);
            y += 30;

            // Summary bar
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.drawCentreString("Ch:" + String(wifiChannel) + " | " + String(uniqueDevs) + " devices nearby", tft.width() / 2, tft.height() - 15, 1);
        }

        delay(100);
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    trackedDevices.clear();
    ledFeedbackRestore();
}

// ── Menu ───────────────────────────────────────────────────────────────────────

void wifiProbeMenu() {
    options = {
        {"Probe Tracker",  wifiProbeTracker   },
        {"Client Tracker", wifiClientTracker  },
        {"SIGINT Dashboard",signalIntelDashboard},
    };
    addOptionToMainMenu();
    loopOptions(options);
}

#endif // LITE_VERSION
