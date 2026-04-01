#include "led_feedback.h"

#ifdef HAS_RGB_LED
#include "led_control.h"
#include <FastLED.h>

// The LED array is owned by led_control.cpp
extern CRGB leds[];

// Current feedback state
static LedFeedbackMode currentMode = LED_FB_IDLE;
static int currentValue = 0;

// Flash state for non-blocking flash
static unsigned long flashEndTime = 0;
static bool flashActive = false;

// Spinner position counter (incremented each call for scan animations)
static uint8_t spinnerPos = 0;

// --- Helper: map RSSI to number of LEDs (1-8) ---
// -30 dBm = 8 LEDs (strong), -90 dBm = 1 LED (weak)
static int rssiToLedCount(int rssi) {
    if (rssi >= -30) return LED_COUNT;
    if (rssi <= -90) return 1;
    // Linear map: -90 -> 1, -30 -> 8
    return 1 + (int)(((float)(rssi + 90) / 60.0f) * (LED_COUNT - 1));
}

// --- Helper: color for signal strength (green=strong, yellow=mid, red=weak) ---
static CRGB rssiColor(int ledIndex, int totalLit) {
    // ledIndex 0 = first lit LED (weakest end), totalLit-1 = strongest
    float ratio = (float)ledIndex / (float)(LED_COUNT - 1);
    // ratio 0.0 = first LED position, 1.0 = last LED position
    // Green when ratio is high (strong), red when low (weak)
    uint8_t r = (uint8_t)(255 * (1.0f - ratio));
    uint8_t g = (uint8_t)(255 * ratio);
    return CRGB(r, g, 0);
}

// --- Helper: spinner animation (one bright LED chasing around the ring) ---
static void showSpinner(CRGB color) {
    fill_solid(leds, LED_COUNT, CRGB::Black);
    // Bright lead LED
    leds[spinnerPos % LED_COUNT] = color;
    // Dimmer trailing LED
    uint8_t trail = (spinnerPos + LED_COUNT - 1) % LED_COUNT;
    leds[trail] = CRGB(color.r / 4, color.g / 4, color.b / 4);
    spinnerPos = (spinnerPos + 1) % LED_COUNT;
    FastLED.show();
}

// --- Helper: pulse animation (all LEDs breathe a color) ---
static void showPulse(CRGB color) {
    float phase = (sinf(millis() / 200.0f) + 1.0f) / 2.0f; // 0.0 - 1.0
    uint8_t brightness = (uint8_t)(phase * 255);
    CRGB dimmed = CRGB(
        (color.r * brightness) / 255,
        (color.g * brightness) / 255,
        (color.b * brightness) / 255
    );
    fill_solid(leds, LED_COUNT, dimmed);
    FastLED.show();
}

// --- Helper: outward pulse (LEDs light from center outward) ---
static void showOutwardPulse(CRGB color) {
    float phase = fmodf(millis() / 300.0f, 1.0f); // 0.0 - 1.0 repeating
    int litCount = (int)(phase * LED_COUNT);
    fill_solid(leds, LED_COUNT, CRGB::Black);
    for (int i = 0; i < litCount && i < LED_COUNT; i++) {
        leds[i] = color;
    }
    FastLED.show();
}

void ledFeedbackInit() {
    currentMode = LED_FB_IDLE;
    currentValue = 0;
    flashActive = false;
    flashEndTime = 0;
    spinnerPos = 0;
}

void ledFeedbackSetMode(LedFeedbackMode mode) {
    currentMode = mode;
    currentValue = 0;
    spinnerPos = 0;
}

void ledFeedbackUpdate(int value) {
    // Check if a flash has expired and restore
    if (flashActive && millis() >= flashEndTime) {
        flashActive = false;
        ledFeedbackRestore();
        return;
    }
    if (flashActive) return; // Don't update while flashing

    currentValue = value;

    switch (currentMode) {
        case LED_FB_IDLE:
            // Do nothing - let the user's normal LED effect run
            break;

        case LED_FB_RF_SIGNAL:
            ledFeedbackShowRSSI(value);
            break;

        case LED_FB_RF_SCAN:
            showSpinner(CRGB::Green);
            break;

        case LED_FB_RF_TX:
            showOutwardPulse(CRGB::Green);
            break;

        case LED_FB_RF_BRUTEFORCE:
            ledFeedbackShowProgress(value);
            break;

        case LED_FB_WIFI_ATTACK:
            showPulse(CRGB::Red);
            break;

        case LED_FB_WIFI_SCAN:
            showSpinner(CRGB::Blue);
            break;

        case LED_FB_BLE_SCAN:
            showSpinner(CRGB::Cyan);
            break;

        case LED_FB_RFID_READ:
            showSpinner(CRGB::Yellow);
            break;

        case LED_FB_RFID_WRITE:
            showPulse(CRGB(255, 191, 0)); // Amber
            break;

        case LED_FB_RFID_BRUTE:
            ledFeedbackShowProgress(value);
            break;

        case LED_FB_IR_TX:
            showPulse(CRGB::Purple);
            break;

        case LED_FB_IR_RX:
            showPulse(CRGB::Blue);
            break;

        case LED_FB_SUCCESS:
            fill_solid(leds, LED_COUNT, CRGB::Green);
            FastLED.show();
            break;

        case LED_FB_FAILURE:
            fill_solid(leds, LED_COUNT, CRGB::Red);
            FastLED.show();
            break;

        case LED_FB_BATTERY: {
            // value = battery percentage 0-100
            CRGB color;
            if (value > 60) color = CRGB::Green;
            else if (value > 30) color = CRGB::Yellow;
            else color = CRGB::Red;

            int litCount = (int)(value / 12.5f);
            if (litCount < 0) litCount = 0;
            if (litCount > LED_COUNT) litCount = LED_COUNT;

            fill_solid(leds, LED_COUNT, CRGB::Black);
            for (int i = 0; i < litCount; i++) {
                leds[i] = color;
            }
            FastLED.show();
            break;
        }

        case LED_FB_FREQ_BAND:
            // value is not used here; use ledFeedbackShowFreqBand() instead
            break;
    }
}

void ledFeedbackFlash(LedFeedbackMode mode, int durationMs) {
    flashActive = true;
    flashEndTime = millis() + durationMs;

    // Show the flash immediately
    switch (mode) {
        case LED_FB_SUCCESS:
            fill_solid(leds, LED_COUNT, CRGB::Green);
            break;
        case LED_FB_FAILURE:
            fill_solid(leds, LED_COUNT, CRGB::Red);
            break;
        case LED_FB_IR_TX:
            fill_solid(leds, LED_COUNT, CRGB::Purple);
            break;
        case LED_FB_IR_RX:
            fill_solid(leds, LED_COUNT, CRGB::Blue);
            break;
        default:
            fill_solid(leds, LED_COUNT, CRGB::White);
            break;
    }
    FastLED.show();
}

void ledFeedbackShowFreqBand(float freqMHz) {
    CRGB color;
    if (freqMHz < 350.0f) {
        color = CRGB::Red;        // 315 MHz band
    } else if (freqMHz < 450.0f) {
        color = CRGB::Green;      // 433 MHz band
    } else if (freqMHz < 800.0f) {
        color = CRGB::Blue;       // 868 MHz band
    } else {
        color = CRGB::Purple;     // 915 MHz band
    }
    fill_solid(leds, LED_COUNT, color);
    FastLED.show();
}

void ledFeedbackShowRSSI(int rssi) {
    int litCount = rssiToLedCount(rssi);

    fill_solid(leds, LED_COUNT, CRGB::Black);
    for (int i = 0; i < litCount && i < LED_COUNT; i++) {
        leds[i] = rssiColor(i, litCount);
    }
    FastLED.show();
}

void ledFeedbackShowProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    // Fill LEDs clockwise based on percentage
    int litCount = (percent * LED_COUNT) / 100;

    fill_solid(leds, LED_COUNT, CRGB::Black);
    for (int i = 0; i < litCount && i < LED_COUNT; i++) {
        // Gradient from blue (start) to green (complete)
        uint8_t g = (uint8_t)((255 * i) / (LED_COUNT - 1));
        uint8_t b = (uint8_t)(255 - g);
        leds[i] = CRGB(0, g, b);
    }
    FastLED.show();
}

void ledFeedbackRestore() {
    currentMode = LED_FB_IDLE;
    flashActive = false;
    // Restore the user's normal LED configuration
    ledSetup();
}

#endif
