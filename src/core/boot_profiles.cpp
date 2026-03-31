#include "boot_profiles.h"

#ifdef T_EMBED_1101

#include <globals.h>
#include "core/display.h"
#include "core/configPins.h"

static std::vector<BootProfile> profiles = {
    {"Normal Boot", "Standard menu", nullptr},  // nullptr = normal boot
    {"RF Hunter", "CC1101 scan + spectrum", []() {
        bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    }},
    {"Wardriver", "WiFi + GPS logging", []() {
        // Will launch wardriving mode
    }},
    {"NFC Reader", "Quick RFID read", []() {
        bruceConfigPins.rfidModule = PN532_I2C_MODULE;
    }},
    {"Passive Recon", "WiFi sniffer mode", nullptr},
    {"IR Blaster", "IR transmit mode", nullptr},
    {"BLE Scanner", "BLE device scan", nullptr},
    {"Red Team", "Evil Portal + Deauth", nullptr},
};

const std::vector<BootProfile>& getBootProfiles() {
    return profiles;
}

// Draw the profile selector UI on the 170x320 ST7789 display
static void drawProfileSelector(int selectedIdx, float countdownRemaining) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("BOOT PROFILES", tftWidth / 2, 5, 2);

    int startY = 35;
    int itemHeight = 28;

    for (int i = 0; i < (int)profiles.size(); i++) {
        int y = startY + i * itemHeight;
        if (i == selectedIdx) {
            tft.fillRoundRect(5, y - 2, tftWidth - 10, itemHeight - 4, 4, bruceConfig.priColor);
            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
        } else {
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        }
        tft.drawCentreString(profiles[i].name, tftWidth / 2, y + 2, 2);
        if (i == selectedIdx) {
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawCentreString(profiles[i].description, tftWidth / 2, y + itemHeight + 2, 1);
        }
    }

    // Draw countdown timer at the bottom
    int timerY = tftHeight - 18;
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    String timerStr = "Auto-boot: " + String((int)countdownRemaining + 1) + "s";
    tft.drawCentreString(timerStr, tftWidth / 2, timerY, 1);
}

// Read encoder direction using direct GPIO polling.
// Returns -1 (CCW), 0 (no change), or +1 (CW).
static int readEncoderDelta() {
    static int lastA = -1;
    static int lastB = -1;

    int a = digitalRead(ENCODER_INA);
    int b = digitalRead(ENCODER_INB);

    if (lastA == -1) {
        // First call: initialize state
        lastA = a;
        lastB = b;
        return 0;
    }

    int delta = 0;
    // Detect transition on A
    if (a != lastA) {
        // CW: A leads B (A changes and differs from B)
        if (a != b) {
            delta = 1;
        } else {
            delta = -1;
        }
    }

    lastA = a;
    lastB = b;
    return delta;
}

bool checkBootProfileSelection() {
    // Configure encoder and button pins for direct GPIO reads
    pinMode(SEL_BTN, INPUT);
    pinMode(ENCODER_INA, INPUT_PULLUP);
    pinMode(ENCODER_INB, INPUT_PULLUP);
    pinMode(BK_BTN, INPUT);

    // Check if encoder button (SEL_BTN / GPIO 0) is held at boot
    // Must be held for >500ms to trigger profile selection
    if (digitalRead(SEL_BTN) != BTN_ACT) {
        return false;
    }

    unsigned long holdStart = millis();
    while (millis() - holdStart < 500) {
        if (digitalRead(SEL_BTN) != BTN_ACT) {
            return false;  // Released too early
        }
        delay(10);
    }

    // Button held for 500ms - enter profile selection mode
    // Wait for the button to be released before entering the selector
    while (digitalRead(SEL_BTN) == BTN_ACT) {
        delay(10);
    }
    delay(50);  // Debounce

    int selectedIdx = 0;
    unsigned long lastInputTime = millis();
    const unsigned long autoBootTimeout = 3000;  // 3 seconds
    bool needsRedraw = true;

    while (true) {
        // Calculate countdown
        unsigned long elapsed = millis() - lastInputTime;
        float countdownRemaining = (float)(autoBootTimeout - elapsed) / 1000.0f;

        // Auto-boot on timeout
        if (elapsed >= autoBootTimeout) {
            // Timeout: normal boot (profile index 0)
            selectedIdx = 0;
            break;
        }

        // Redraw periodically for the countdown timer (every ~250ms) or on input
        static unsigned long lastDrawTime = 0;
        if (needsRedraw || (millis() - lastDrawTime > 250)) {
            drawProfileSelector(selectedIdx, countdownRemaining);
            lastDrawTime = millis();
            needsRedraw = false;
        }

        // Read encoder rotation
        int delta = readEncoderDelta();
        if (delta != 0) {
            selectedIdx += delta;
            // Wrap around
            if (selectedIdx < 0) selectedIdx = (int)profiles.size() - 1;
            if (selectedIdx >= (int)profiles.size()) selectedIdx = 0;
            lastInputTime = millis();  // Reset timeout on input
            needsRedraw = true;
        }

        // Check encoder press (select)
        if (digitalRead(SEL_BTN) == BTN_ACT) {
            delay(50);  // Debounce
            while (digitalRead(SEL_BTN) == BTN_ACT) {
                delay(10);  // Wait for release
            }
            delay(50);  // Debounce
            break;
        }

        // Check back button (cancel = normal boot)
        if (digitalRead(BK_BTN) == BTN_ACT) {
            delay(50);  // Debounce
            while (digitalRead(BK_BTN) == BTN_ACT) {
                delay(10);  // Wait for release
            }
            selectedIdx = 0;  // Normal boot
            break;
        }

        delay(5);  // Small delay to avoid busy-spinning
    }

    // Activate selected profile
    if (profiles[selectedIdx].activate != nullptr) {
        profiles[selectedIdx].activate();
    }

    // Clear the screen after selection
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString(profiles[selectedIdx].name, tftWidth / 2, tftHeight / 2 - 10, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Selected", tftWidth / 2, tftHeight / 2 + 15, 1);
    delay(800);

    // Return true if a non-default profile was selected
    return (profiles[selectedIdx].activate != nullptr);
}

#endif // T_EMBED_1101
