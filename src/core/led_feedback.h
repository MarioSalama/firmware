#ifndef __LED_FEEDBACK_H__
#define __LED_FEEDBACK_H__

#include <globals.h>
#ifdef HAS_RGB_LED
#include <FastLED.h>

// LED feedback modes tied to active modules
enum LedFeedbackMode {
    LED_FB_IDLE = 0,        // Default: show user's chosen effect
    LED_FB_RF_SIGNAL,       // RF: signal strength gradient (green->red)
    LED_FB_RF_SCAN,         // RF: spinning animation while scanning
    LED_FB_RF_TX,           // RF: pulse outward while transmitting
    LED_FB_RF_BRUTEFORCE,   // RF: progress bar around ring
    LED_FB_WIFI_ATTACK,     // WiFi: pulse red while attacking
    LED_FB_WIFI_SCAN,       // WiFi: blue spinner
    LED_FB_BLE_SCAN,        // BLE: cyan spinner
    LED_FB_RFID_READ,       // RFID: spinner, flash green on success
    LED_FB_RFID_WRITE,      // RFID: amber pulse while writing
    LED_FB_RFID_BRUTE,      // RFID: progress bar
    LED_FB_IR_TX,           // IR: flash purple on transmit
    LED_FB_IR_RX,           // IR: flash blue on receive
    LED_FB_SUCCESS,         // Generic: all green flash
    LED_FB_FAILURE,         // Generic: all red flash
    LED_FB_BATTERY,         // Battery level indicator
    LED_FB_FREQ_BAND,       // Frequency band color indicator
};

// Initialize the feedback system
void ledFeedbackInit();

// Set the current feedback mode
void ledFeedbackSetMode(LedFeedbackMode mode);

// Update with a value (e.g., RSSI, progress 0-100, battery %)
void ledFeedbackUpdate(int value);

// Quick flash feedback (non-blocking, returns immediately)
void ledFeedbackFlash(LedFeedbackMode mode, int durationMs = 200);

// Show frequency band on LEDs (315=red, 433=green, 868=blue, 915=purple)
void ledFeedbackShowFreqBand(float freqMHz);

// Show signal strength as LED gradient (0-8 LEDs lit, green to red)
void ledFeedbackShowRSSI(int rssi);

// Show progress (0-100) as filled ring
void ledFeedbackShowProgress(int percent);

// Return to user's normal LED effect
void ledFeedbackRestore();

#else
// No-op stubs when no RGB LED
inline void ledFeedbackInit() {}
inline void ledFeedbackSetMode(int mode) {}
inline void ledFeedbackUpdate(int value) {}
inline void ledFeedbackFlash(int mode, int durationMs = 200) {}
inline void ledFeedbackShowFreqBand(float freqMHz) {}
inline void ledFeedbackShowRSSI(int rssi) {}
inline void ledFeedbackShowProgress(int percent) {}
inline void ledFeedbackRestore() {}
#endif

#endif
