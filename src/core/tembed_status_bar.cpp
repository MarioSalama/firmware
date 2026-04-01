#include "tembed_status_bar.h"

#ifdef T_EMBED_1101

#include "core/display.h"
#include "core/led_control.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <WiFi.h>
#include <globals.h>

// ── Enhanced Status Bar ─────────────────────────────────────────────────────

void drawFreqIndicator(float freqMHz) {
    // Small frequency display in the status bar area
    if (freqMHz <= 0) return;

    // Color code by band
    uint16_t freqColor;
    if (freqMHz < 350) freqColor = TFT_RED;
    else if (freqMHz < 450) freqColor = TFT_GREEN;
    else if (freqMHz < 800) freqColor = TFT_BLUE;
    else freqColor = TFT_MAGENTA;

    tft.setTextColor(freqColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    // Draw in top-left area after "Bruce" text
    String freqStr = String(freqMHz, 1);
    tft.drawString(freqStr, 5, 14, 1);
}

void drawSignalBars(int x, int y, int strength, uint16_t color) {
    // Draw 4 signal strength bars (like mobile phone)
    // strength: 0-4
    int barW = 3;
    int gap = 1;
    int maxH = 10;

    for (int i = 0; i < 4; i++) {
        int barH = (maxH * (i + 1)) / 4;
        int barX = x + i * (barW + gap);
        int barY = y + maxH - barH;

        if (i < strength) {
            tft.fillRect(barX, barY, barW, barH, color);
        } else {
            tft.drawRect(barX, barY, barW, barH, bruceConfig.priColor);
        }
    }
}

void drawEnhancedStatusBar() {
    // First draw the standard status bar
    drawStatusBar();

    // Add CC1101 frequency if set
    if (bruceConfigPins.rfFreq > 0) {
        drawFreqIndicator(bruceConfigPins.rfFreq);
    }
}

// ── Multi-Radio Dashboard ───────────────────────────────────────────────────

struct RadioStatus {
    const char *name;
    const char *icon;
    bool active;
    String info;
    uint16_t color;
};

void showRadioDashboard() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Radio Dashboard", tftWidth / 2, 5, 2);

    // Gather status of all radios
    RadioStatus radios[] = {
        {"WiFi", "W", WiFi.getMode() != WIFI_OFF,
         WiFi.isConnected() ? WiFi.SSID() + " " + String(WiFi.RSSI()) + "dBm"
                            : (WiFi.getMode() == WIFI_AP ? "AP Mode" : "Off"),
         WiFi.isConnected() ? TFT_GREEN : (WiFi.getMode() ? TFT_YELLOW : TFT_DARKGREY)},

        {"BLE", "B", BLEConnected, BLEConnected ? "Connected" : "Off",
         BLEConnected ? TFT_CYAN : TFT_DARKGREY},

        {"CC1101", "R", bruceConfigPins.rfModule == CC1101_SPI_MODULE,
         bruceConfigPins.rfModule == CC1101_SPI_MODULE
             ? String(bruceConfigPins.rfFreq, 2) + " MHz"
             : "Not init",
         bruceConfigPins.rfModule == CC1101_SPI_MODULE ? TFT_GREEN : TFT_DARKGREY},

        {"NRF24", "N", false, "Available", TFT_DARKGREY},

        {"IR", "I", true, "TX:" + String(bruceConfigPins.irTx) + " RX:" + String(bruceConfigPins.irRx),
         TFT_MAGENTA},

        {"NFC", "F", bruceConfigPins.rfidModule == PN532_I2C_MODULE, "PN532 I2C",
         bruceConfigPins.rfidModule == PN532_I2C_MODULE ? TFT_YELLOW : TFT_DARKGREY},
    };

    int radioCount = 6;
    int cardH = 20;
    int cardGap = 3;
    int startY = 28;

    for (int i = 0; i < radioCount; i++) {
        int y = startY + i * (cardH + cardGap);
        RadioStatus &r = radios[i];

        // Background card
        uint16_t bgColor = r.active ? tft.color565(20, 30, 20) : tft.color565(15, 15, 15);
        tft.fillRoundRect(5, y, tftWidth - 10, cardH, 3, bgColor);

        // Status dot
        tft.fillCircle(15, y + cardH / 2, 4, r.color);

        // Radio name
        tft.setTextColor(bruceConfig.priColor, bgColor);
        tft.drawString(r.name, 25, y + 3, 1);

        // Status info (right-aligned)
        tft.setTextColor(r.color, bgColor);
        int infoW = tft.textWidth(r.info.c_str(), 1);
        tft.drawString(r.info.c_str(), tftWidth - 15 - infoW, y + 3, 1);
    }

    // Footer
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Press [BACK] to exit", tftWidth / 2, tftHeight - 15, 1);

    // Wait for exit
    while (true) {
        if (check(EscPress) || check(SelPress)) return;
        delay(100);
    }
}

// ── Quick Settings Overlay ──────────────────────────────────────────────────

void showQuickSettings() {
    // Semi-transparent overlay effect (darken screen)
    int overlayX = 20;
    int overlayY = 20;
    int overlayW = tftWidth - 40;
    int overlayH = tftHeight - 40;

    tft.fillRoundRect(overlayX, overlayY, overlayW, overlayH, 8, tft.color565(10, 10, 10));
    tft.drawRoundRect(overlayX, overlayY, overlayW, overlayH, 8, bruceConfig.priColor);

    int y = overlayY + 10;
    int x = overlayX + 10;
    int lineH = 22;

    tft.setTextColor(bruceConfig.priColor, tft.color565(10, 10, 10));
    tft.drawCentreString("Quick Settings", tftWidth / 2, y, 2);
    y += lineH + 5;

    // Brightness
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("Brightness:", x, y, 1);
    tft.setTextColor(bruceConfig.priColor, tft.color565(10, 10, 10));
    tft.drawString(String(bruceConfig.bright) + "%", x + 90, y, 1);
    // Draw brightness bar
    int barX = x + 130;
    int barW = overlayW - 150;
    tft.drawRect(barX, y, barW, 10, bruceConfig.priColor);
    tft.fillRect(barX + 1, y + 1, (barW - 2) * bruceConfig.bright / 100, 8,
                 bruceConfig.priColor);
    y += lineH;

    // Volume
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("Volume:", x, y, 1);
    tft.setTextColor(bruceConfig.priColor, tft.color565(10, 10, 10));
    tft.drawString(
        bruceConfig.soundEnabled ? String(bruceConfig.soundVolume) + "%" : "Muted", x + 90, y, 1
    );
    y += lineH;

    // Battery
    int bat = getBattery();
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("Battery:", x, y, 1);
    uint16_t batColor = bat > 60 ? TFT_GREEN : bat > 30 ? TFT_YELLOW : TFT_RED;
    tft.setTextColor(batColor, tft.color565(10, 10, 10));
    tft.drawString(String(bat) + "%", x + 90, y, 1);
    y += lineH;

    // RF Frequency
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("RF Freq:", x, y, 1);
    tft.setTextColor(bruceConfig.priColor, tft.color565(10, 10, 10));
    tft.drawString(String(bruceConfigPins.rfFreq, 2) + " MHz", x + 90, y, 1);
    y += lineH;

    // LED
#ifdef HAS_RGB_LED
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("LED:", x, y, 1);
    tft.setTextColor(bruceConfig.priColor, tft.color565(10, 10, 10));
    tft.drawString(
        bruceConfig.ledBright > 0 ? String(bruceConfig.ledBright) + "%" : "Off", x + 90, y, 1
    );
    y += lineH;
#endif

    // SD Card
    tft.setTextColor(bruceConfig.secColor, tft.color565(10, 10, 10));
    tft.drawString("SD Card:", x, y, 1);
    tft.setTextColor(sdcardMounted ? TFT_GREEN : TFT_RED, tft.color565(10, 10, 10));
    tft.drawString(sdcardMounted ? "Mounted" : "Not found", x + 90, y, 1);

    // Wait for dismiss
    while (true) {
        if (check(EscPress) || check(SelPress)) return;
        delay(100);
    }
}

#endif // T_EMBED_1101
