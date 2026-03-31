#include "encoder_gestures.h"

#ifdef T_EMBED_1101

#include <globals.h>
#include "core/display.h"
#include <RotaryEncoder.h>

extern RotaryEncoder *encoder;

// ---------------------------------------------------------------------------
// Gesture detection state
// ---------------------------------------------------------------------------
static unsigned long pressTime = 0;
static unsigned long releaseTime = 0;
static int clickCount = 0;
static bool wasPressed = false;
static int lastEncoderPos = 0;
static unsigned long lastRotateTime = 0;

// Registered callbacks
static GestureCallback doubleClickCb = nullptr;
static GestureCallback longPressCb = nullptr;
static GestureCallback twistClickCb = nullptr;

// ---------------------------------------------------------------------------
// Recent tools storage
// ---------------------------------------------------------------------------
struct RecentTool {
    String name;
    GestureCallback action;
};
static std::vector<RecentTool> recentTools;
static const int MAX_RECENT = 8;

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
void encoderGesturesInit() {
    pressTime = 0;
    releaseTime = 0;
    clickCount = 0;
    wasPressed = false;
    lastEncoderPos = 0;
    lastRotateTime = 0;
    doubleClickCb = nullptr;
    longPressCb = nullptr;
    twistClickCb = nullptr;
    if (encoder) {
        lastEncoderPos = encoder->getPosition();
    }
}

// ---------------------------------------------------------------------------
// Gesture polling  (call from main input loop)
// ---------------------------------------------------------------------------
EncoderGesture encoderGesturePoll() {
    bool pressed = (digitalRead(ENCODER_KEY) == LOW); // Active low
    unsigned long now = millis();

    // Track encoder rotation
    if (encoder) {
        int pos = encoder->getPosition();
        if (pos != lastEncoderPos) {
            lastRotateTime = now;
            lastEncoderPos = pos;
        }
    }

    // Button just pressed
    if (pressed && !wasPressed) {
        pressTime = now;
        wasPressed = true;
    }

    // Button just released
    if (!pressed && wasPressed) {
        wasPressed = false;
        unsigned long pressDuration = now - pressTime;

        // Long press detected on release
        if (pressDuration > 800) {
            clickCount = 0;
            return GESTURE_LONG_PRESS;
        }

        // Short press – check for twist-click first
        if (pressDuration < 300) {
            if (now - lastRotateTime < 300) {
                clickCount = 0;
                return GESTURE_TWIST_CLICK;
            }
            clickCount++;
            releaseTime = now;
        }
    }

    // Long press detected while still held
    if (pressed && wasPressed && (now - pressTime > 800)) {
        wasPressed = false; // Consume the long press
        clickCount = 0;
        return GESTURE_LONG_PRESS;
    }

    // Evaluate accumulated clicks after the double-click window expires
    if (clickCount > 0 && (now - releaseTime > 400)) {
        if (clickCount >= 2) {
            clickCount = 0;
            return GESTURE_DOUBLE_CLICK;
        }
        clickCount = 0;
        return GESTURE_SINGLE_CLICK;
    }

    return GESTURE_NONE;
}

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------
void onDoubleClick(GestureCallback cb) { doubleClickCb = cb; }
void onLongPress(GestureCallback cb)   { longPressCb = cb; }
void onTwistClick(GestureCallback cb)  { twistClickCb = cb; }

// ---------------------------------------------------------------------------
// Recent tools management
// ---------------------------------------------------------------------------
void addRecentTool(const char* name, GestureCallback action) {
    // Remove if already exists
    recentTools.erase(
        std::remove_if(recentTools.begin(), recentTools.end(),
            [name](const RecentTool& t) { return t.name == name; }),
        recentTools.end()
    );
    // Add to front
    recentTools.insert(recentTools.begin(), {String(name), action});
    // Trim to max
    if ((int)recentTools.size() > MAX_RECENT) {
        recentTools.resize(MAX_RECENT);
    }
}

// ---------------------------------------------------------------------------
// Favorites Wheel  (long press – circular menu of recent tools)
// ---------------------------------------------------------------------------
void showFavoritesWheel() {
    if (recentTools.empty()) return;

    int selected = 0;
    int count = (int)recentTools.size();
    int basePos = encoder ? encoder->getPosition() : 0;
    unsigned long lastActivity = millis();
    const unsigned long TIMEOUT_MS = 5000;

    // Centre of display
    int cx = tftWidth / 2;
    int cy = tftHeight / 2;
    int radius = min(cx, cy) - 20;

    // Save background by drawing overlay
    tft.fillScreen(bruceConfig.bgColor);

    while (true) {
        // Timeout check
        if (millis() - lastActivity > TIMEOUT_MS) break;

        // Back button dismisses
        if (digitalRead(BK_BTN) == BTN_ACT) {
            while (digitalRead(BK_BTN) == BTN_ACT) delay(10); // wait release
            break;
        }

        // Encoder rotation changes selection
        if (encoder) {
            int pos = encoder->getPosition();
            int diff = pos - basePos;
            if (diff != 0) {
                selected = ((selected + diff) % count + count) % count;
                basePos = pos;
                lastActivity = millis();
            }
        }

        // Select button launches tool
        if (digitalRead(ENCODER_KEY) == LOW) {
            unsigned long pt = millis();
            while (digitalRead(ENCODER_KEY) == LOW) delay(10);
            if (millis() - pt < 800) { // short press = select
                tft.fillScreen(bruceConfig.bgColor);
                if (recentTools[selected].action) {
                    recentTools[selected].action();
                }
                return;
            }
        }

        // Draw the wheel
        tft.fillScreen(bruceConfig.bgColor);

        // Title
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Favorites", cx, 4, 1);

        // Draw items in a circle
        for (int i = 0; i < count; i++) {
            float angle = (2.0f * PI * i / count) - (PI / 2.0f); // start at top
            int ix = cx + (int)(radius * cos(angle));
            int iy = cy + (int)(radius * sin(angle));

            if (i == selected) {
                tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
                int tw = tft.textWidth(recentTools[i].name.c_str(), 1);
                int th = tft.fontHeight(1);
                tft.fillRoundRect(ix - tw / 2 - 4, iy - th / 2 - 2,
                                  tw + 8, th + 4, 4, bruceConfig.priColor);
            } else {
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            }
            tft.drawCentreString(recentTools[i].name.c_str(), ix, iy - tft.fontHeight(1) / 2, 1);
        }

        // Small centre indicator
        tft.fillCircle(cx, cy, 3, bruceConfig.priColor);

        delay(50); // ~20 fps
    }

    // Restore screen
    tft.fillScreen(bruceConfig.bgColor);
    drawStatusBar();
}

// ---------------------------------------------------------------------------
// Quick Switcher  (double click – toggle between last 2 tools)
// ---------------------------------------------------------------------------
void showQuickSwitcher() {
    if (recentTools.size() < 2) return;

    int selected = 0;
    unsigned long startTime = millis();
    const unsigned long TIMEOUT_MS = 3000;
    int basePos = encoder ? encoder->getPosition() : 0;

    // Overlay dimensions
    int boxW = tftWidth - 40;
    int boxH = 52;
    int boxX = 20;
    int boxY = (tftHeight - boxH) / 2;

    while (true) {
        if (millis() - startTime > TIMEOUT_MS) break;

        // Back button dismisses
        if (digitalRead(BK_BTN) == BTN_ACT) {
            while (digitalRead(BK_BTN) == BTN_ACT) delay(10);
            break;
        }

        // Encoder toggles selection
        if (encoder) {
            int pos = encoder->getPosition();
            if (pos != basePos) {
                selected = (selected == 0) ? 1 : 0;
                basePos = pos;
                startTime = millis(); // reset timeout on interaction
            }
        }

        // Select button launches
        if (digitalRead(ENCODER_KEY) == LOW) {
            unsigned long pt = millis();
            while (digitalRead(ENCODER_KEY) == LOW) delay(10);
            if (millis() - pt < 800) {
                tft.fillRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, bruceConfig.bgColor);
                if (recentTools[selected].action) {
                    recentTools[selected].action();
                }
                return;
            }
        }

        // Draw compact overlay
        tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, bruceConfig.bgColor);
        tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, bruceConfig.priColor);

        int itemH = boxH / 2;
        for (int i = 0; i < 2; i++) {
            int iy = boxY + i * itemH;
            if (i == selected) {
                tft.fillRoundRect(boxX + 2, iy + 2, boxW - 4, itemH - 4, 4, bruceConfig.priColor);
                tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            } else {
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            }
            tft.drawCentreString(
                recentTools[i].name.c_str(),
                boxX + boxW / 2,
                iy + (itemH - tft.fontHeight(1)) / 2,
                1
            );
        }

        delay(50);
    }

    // Clean up overlay
    tft.fillRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, bruceConfig.bgColor);
    drawStatusBar();
}

#endif // T_EMBED_1101
