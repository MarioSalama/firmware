#include "battery_dashboard.h"

#ifdef USE_BQ27220_VIA_I2C

#include <globals.h>
#include "core/display.h"
#include <bq27220.h>

BatteryInfo getBatteryInfo() {
    BatteryInfo info;
    info.chargePercent      = bq.getChargePcnt();
    info.voltage            = bq.getVolt(VOLT);
    info.current            = bq.getCurr(CURR_INSTANT);
    info.temperature        = bq.getTemp();
    info.remainingCapacity  = bq.getRemainCap();
    info.fullChargeCapacity = bq.getFullChargeCap();
    info.designCapacity     = bq.getDesignCap();
    info.isCharging         = bq.getIsCharging();
    info.timeToEmpty        = bq.getTimeToEmpty();
    info.cycleCount         = 0;  // BQ27220 does not expose cycle count directly
    info.timeToFull         = 0;  // Not available on BQ27220

    // Calculate health as percentage of full charge capacity vs design capacity
    if (info.designCapacity > 0) {
        info.healthPercent = (info.fullChargeCapacity * 100) / info.designCapacity;
    } else {
        info.healthPercent = 0;
    }

    return info;
}

int estimateRuntime(const char* mode) {
    BatteryInfo info = getBatteryInfo();
    if (info.current >= 0) return 0; // charging or no draw

    int absCurrent = -info.current;
    if (absCurrent == 0) return 0;

    return (info.remainingCapacity * 60) / absCurrent;
}

void showBatteryDashboard() {
    bool running = true;

    while (running) {
        BatteryInfo info = getBatteryInfo();

        tft.fillScreen(bruceConfig.bgColor);

        // --- Draw battery icon (left portion) ---
        int batX = 15, batY = 25, batW = 60, batH = 110;
        int batTipW = 20, batTipH = 8;

        // Battery outline
        tft.drawRoundRect(batX, batY + batTipH, batW, batH, 4, bruceConfig.priColor);
        // Battery tip (positive terminal)
        tft.fillRoundRect(batX + (batW - batTipW) / 2, batY, batTipW, batTipH + 2, 2, bruceConfig.priColor);

        // Fill level
        int fillH = (batH - 6) * info.chargePercent / 100;
        uint16_t fillColor = info.chargePercent > 60 ? TFT_GREEN :
                             info.chargePercent > 30 ? TFT_YELLOW : TFT_RED;
        if (fillH > 0) {
            tft.fillRoundRect(batX + 3, batY + batTipH + 3 + (batH - 6 - fillH),
                              batW - 6, fillH, 2, fillColor);
        }

        // Charging bolt icon overlay
        if (info.isCharging) {
            int cx = batX + batW / 2;
            int cy = batY + batTipH + batH / 2;
            // Simple lightning bolt using lines
            tft.drawLine(cx + 2, cy - 15, cx - 5, cy + 2, TFT_WHITE);
            tft.drawLine(cx - 5, cy + 2, cx + 3, cy + 2, TFT_WHITE);
            tft.drawLine(cx + 3, cy + 2, cx - 2, cy + 15, TFT_WHITE);
        }

        // Charge percent big text below battery
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString(String(info.chargePercent) + "%", batX + batW / 2, batY + batH + batTipH + 5, 4);

        // --- Stats panel (right side) ---
        int sx = 95, sy = 20, lineH = 18;

        // Voltage
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Voltage:", sx, sy, 1);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString(String(info.voltage / 1000.0, 2) + "V", sx + 60, sy, 1);
        sy += lineH;

        // Current
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Current:", sx, sy, 1);
        tft.setTextColor(info.current >= 0 ? TFT_GREEN : TFT_ORANGE, bruceConfig.bgColor);
        tft.drawString(String(info.current) + "mA", sx + 60, sy, 1);
        sy += lineH;

        // Temperature (BQ27220 returns 0.1K units, convert to Celsius)
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Temp:", sx, sy, 1);
        float tempC = info.temperature / 10.0 - 273.15;
        tft.setTextColor(tempC > 45.0 ? TFT_RED : bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString(String(tempC, 1) + "C", sx + 60, sy, 1);
        sy += lineH;

        // Capacity
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Capacity:", sx, sy, 1);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString(String(info.remainingCapacity) + "/" + String(info.fullChargeCapacity) + "mAh", sx + 60, sy, 1);
        sy += lineH;

        // Health
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Health:", sx, sy, 1);
        tft.setTextColor(info.healthPercent > 80 ? TFT_GREEN : TFT_YELLOW, bruceConfig.bgColor);
        tft.drawString(String(info.healthPercent) + "%", sx + 60, sy, 1);
        sy += lineH;

        // Design capacity
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString("Design:", sx, sy, 1);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString(String(info.designCapacity) + "mAh", sx + 60, sy, 1);
        sy += lineH;

        // --- Bottom bar: charging status or estimated runtime ---
        if (info.isCharging) {
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawCentreString("CHARGING", tftWidth / 2, tftHeight - 15, 1);
        } else if (info.current < 0) {
            int mins = (info.remainingCapacity * 60) / (-info.current);
            int hrs = mins / 60;
            mins = mins % 60;
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawCentreString("Est: " + String(hrs) + "h " + String(mins) + "m remaining",
                                tftWidth / 2, tftHeight - 15, 1);
        }

        // --- Wait for input or 2 second refresh ---
        unsigned long start = millis();
        while (millis() - start < 2000) {
            if (check(EscPress) || check(SelPress)) {
                running = false;
                returnToMenu = true;
                break;
            }
            delay(50);
        }
    }
}

#endif // USE_BQ27220_VIA_I2C
