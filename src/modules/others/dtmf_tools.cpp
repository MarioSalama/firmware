/**
 * @file dtmf_tools.cpp
 * @brief DTMF decoder/encoder, audio bug detector, ultrasonic jammer
 * @details Uses SPM1423 mic for decoding and NS4168 speaker for tone generation
 */

#include "dtmf_tools.h"

#if (defined(MIC_SPM1423) || defined(MIC_INMP441)) && (defined(HAS_NS4168_SPKR) || defined(BUZZ_PIN))

#include "mic.h"
#include "audio.h"
#include "core/display.h"
#include "core/led_feedback.h"
#include "core/mykeyboard.h"
#include <globals.h>
#include <fft.h>

// ── DTMF Frequency Table ───────────────────────────────────────────────────────
// DTMF uses pairs of frequencies: one low group + one high group

static const int DTMF_LOW[]  = {697, 770, 852, 941};   // Row frequencies
static const int DTMF_HIGH[] = {1209, 1336, 1477, 1633}; // Column frequencies

// DTMF keypad layout (rows x cols)
static const char DTMF_KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

// Tone duration for encoding
static constexpr int DTMF_TONE_MS = 200;
static constexpr int DTMF_PAUSE_MS = 100;

// Detection parameters
static constexpr int DTMF_SAMPLE_RATE = 8000;
static constexpr int DTMF_FFT_SIZE = 256;
static constexpr float DTMF_FREQ_TOLERANCE = 25.0f; // Hz tolerance for detection
static constexpr float DTMF_MIN_MAGNITUDE = 500.0f; // Minimum FFT magnitude

// ── Goertzel Algorithm ─────────────────────────────────────────────────────────
// More efficient than full FFT for detecting specific frequencies

static float goertzel(const int16_t *samples, int numSamples, int targetFreq, int sampleRate) {
    float k = 0.5f + ((float)numSamples * targetFreq / sampleRate);
    float w = (2.0f * PI * k) / numSamples;
    float coeff = 2.0f * cos(w);
    float s0 = 0, s1 = 0, s2 = 0;

    for (int i = 0; i < numSamples; i++) {
        s0 = (float)samples[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2);
}

static char detectDTMF(const int16_t *samples, int numSamples, int sampleRate) {
    // Compute Goertzel magnitude for each DTMF frequency
    float lowMag[4], highMag[4];

    for (int i = 0; i < 4; i++) {
        lowMag[i]  = goertzel(samples, numSamples, DTMF_LOW[i], sampleRate);
        highMag[i] = goertzel(samples, numSamples, DTMF_HIGH[i], sampleRate);
    }

    // Find strongest low and high frequency
    int bestLow = 0, bestHigh = 0;
    for (int i = 1; i < 4; i++) {
        if (lowMag[i] > lowMag[bestLow]) bestLow = i;
        if (highMag[i] > highMag[bestHigh]) bestHigh = i;
    }

    // Check if magnitudes are strong enough
    if (lowMag[bestLow] < DTMF_MIN_MAGNITUDE || highMag[bestHigh] < DTMF_MIN_MAGNITUDE) {
        return 0; // No tone detected
    }

    // Check that the best is significantly stronger than others (at least 2x)
    for (int i = 0; i < 4; i++) {
        if (i != bestLow && lowMag[i] > lowMag[bestLow] * 0.5f) return 0;
        if (i != bestHigh && highMag[i] > highMag[bestHigh] * 0.5f) return 0;
    }

    return DTMF_KEYS[bestLow][bestHigh];
}

// ── DTMF Decoder ───────────────────────────────────────────────────────────────

void dtmfDecoder() {
    ledFeedbackSetMode(LED_FB_IR_ACTIVE);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
    tft.drawCentreString("DTMF DECODER", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Listening for tones...", tft.width() / 2, 25, 1);

    // Decoded number display area
    String decoded = "";
    char lastChar = 0;
    int sameCount = 0;
    int noToneCount = 0;

    // Draw keypad reference
    int keyY = tft.height() - 60;
    tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
    tft.drawCentreString("1 2 3 A", tft.width() / 2, keyY, 1);
    tft.drawCentreString("4 5 6 B", tft.width() / 2, keyY + 12, 1);
    tft.drawCentreString("7 8 9 C", tft.width() / 2, keyY + 24, 1);
    tft.drawCentreString("* 0 # D", tft.width() / 2, keyY + 36, 1);

    while (1) {
        if (check(EscPress)) break;

        // Capture audio samples
        int16_t *samples = nullptr;
        uint32_t actualRate = 0;
        if (!mic_capture_samples(DTMF_FFT_SIZE, DTMF_SAMPLE_RATE, 2.0f, &samples, &actualRate)) {
            delay(10);
            continue;
        }

        char tone = detectDTMF(samples, DTMF_FFT_SIZE, actualRate);
        free(samples);

        if (tone != 0) {
            noToneCount = 0;
            if (tone == lastChar) {
                sameCount++;
            } else {
                // New tone detected
                if (decoded.length() < 32) {
                    decoded += tone;
                    ledFeedbackFlash(CRGB::Green, 50);
                }
                sameCount = 1;
                lastChar = tone;
            }

            // Display current tone prominently
            tft.fillRect(0, 45, tft.width(), 40, bruceConfig.bgColor);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            String toneStr = "Tone: ";
            toneStr += tone;
            toneStr += " (";
            // Find frequencies
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    if (DTMF_KEYS[r][c] == tone) {
                        toneStr += String(DTMF_LOW[r]) + "+" + String(DTMF_HIGH[c]) + "Hz";
                    }
                }
            }
            toneStr += ")";
            tft.drawCentreString(toneStr, tft.width() / 2, 50, 1);
        } else {
            noToneCount++;
            if (noToneCount > 5) lastChar = 0; // Reset after silence
        }

        // Display decoded string
        tft.fillRect(0, 90, tft.width(), 30, bruceConfig.bgColor);
        tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
        tft.drawCentreString("Decoded: " + decoded, tft.width() / 2, 95, 1);

        delay(20);
    }

    ledFeedbackRestore();
}

// ── DTMF Encoder ───────────────────────────────────────────────────────────────

static void playDTMFTone(char key) {
    int lowFreq = 0, highFreq = 0;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (DTMF_KEYS[r][c] == key) {
                lowFreq = DTMF_LOW[r];
                highFreq = DTMF_HIGH[c];
            }
        }
    }

    if (lowFreq == 0) return;

    // Play both frequencies simultaneously using rapid alternation
    unsigned long start = millis();
    while (millis() - start < (unsigned long)DTMF_TONE_MS) {
        // Alternate between frequencies rapidly for dual-tone effect
        playTone(lowFreq, 5, 0);
        playTone(highFreq, 5, 0);
    }
}

void dtmfEncoder() {
    // Show dial pad and let user select tones
    const char keys[] = "123A456B789C*0#D";

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("DTMF ENCODER", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Select tone to play", tft.width() / 2, 25, 1);

    options.clear();
    for (int i = 0; i < 16; i++) {
        char k = keys[i];
        String label = "Tone: ";
        label += k;
        options.push_back({label, [k]() {
            ledFeedbackFlash(CRGB::Blue, DTMF_TONE_MS);
            playDTMFTone(k);
            delay(DTMF_PAUSE_MS);
        }});
    }
    addOptionToMainMenu();
    loopOptions(options);
}

// ── DTMF Dialer ────────────────────────────────────────────────────────────────

void dtmfDialer() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.drawCentreString("DTMF DIALER", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Enter number to dial:", tft.width() / 2, 25, 1);

    // Pre-defined useful numbers
    options.clear();
    options.push_back({"Custom Number", []() {
        // Use default - user can type on serial or use encoder
        String number = "5551234";
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
        tft.drawCentreString("Dialing: " + number, tft.width() / 2, 30, 1);

        ledFeedbackSetMode(LED_FB_RF_TX);
        for (int i = 0; i < (int)number.length(); i++) {
            if (check(EscPress)) break;
            char c = number[i];
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.drawCentreString("Playing: " + String(c) + " (" + String(i + 1) + "/" + String(number.length()) + ")", tft.width() / 2, 60, 1);
            playDTMFTone(c);
            delay(DTMF_PAUSE_MS);
        }
        ledFeedbackRestore();
        displayRedStripe("Dial complete!");
        delay(1000);
    }});

    // Common IVR sequences
    options.push_back({"Voicemail (*86)", []() {
        const char *seq = "*86";
        ledFeedbackSetMode(LED_FB_RF_TX);
        for (int i = 0; seq[i]; i++) { playDTMFTone(seq[i]); delay(DTMF_PAUSE_MS); }
        ledFeedbackRestore();
    }});
    options.push_back({"Redial (*69)", []() {
        const char *seq = "*69";
        ledFeedbackSetMode(LED_FB_RF_TX);
        for (int i = 0; seq[i]; i++) { playDTMFTone(seq[i]); delay(DTMF_PAUSE_MS); }
        ledFeedbackRestore();
    }});
    options.push_back({"Block Caller ID (*67)", []() {
        const char *seq = "*67";
        ledFeedbackSetMode(LED_FB_RF_TX);
        for (int i = 0; seq[i]; i++) { playDTMFTone(seq[i]); delay(DTMF_PAUSE_MS); }
        ledFeedbackRestore();
    }});

    addOptionToMainMenu();
    loopOptions(options);
}

// ── Audio Bug Detector ─────────────────────────────────────────────────────────
// Scans for anomalous frequencies that indicate hidden transmitters/bugs

void audioBugDetector() {
    ledFeedbackSetMode(LED_FB_RF_SCAN);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("BUG DETECTOR", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Scanning for audio anomalies", tft.width() / 2, 25, 1);
    tft.drawCentreString("Walk around the room slowly", tft.width() / 2, 40, 1);

    // Baseline noise level
    float baselineLevel = 0;
    int baselineSamples = 5;

    for (int i = 0; i < baselineSamples; i++) {
        int16_t *samples = nullptr;
        uint32_t rate = 0;
        if (mic_capture_samples(1024, 44100, 2.0f, &samples, &rate)) {
            float sum = 0;
            for (int j = 0; j < 1024; j++) sum += abs(samples[j]);
            baselineLevel += sum / 1024.0f;
            free(samples);
        }
    }
    baselineLevel /= baselineSamples;

    tft.fillRect(0, 55, tft.width(), 15, bruceConfig.bgColor);
    tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
    tft.drawCentreString("Baseline: " + String(baselineLevel, 0), tft.width() / 2, 55, 1);

    float maxAnomaly = 0;
    int alertCount = 0;

    while (1) {
        if (check(EscPress)) break;

        int16_t *samples = nullptr;
        uint32_t rate = 0;
        if (!mic_capture_samples(1024, 44100, 2.0f, &samples, &rate)) {
            delay(10);
            continue;
        }

        // Compute average level
        float level = 0;
        for (int j = 0; j < 1024; j++) level += abs(samples[j]);
        level /= 1024.0f;

        // Check for ultrasonic activity (above 18kHz)
        // Use Goertzel on specific suspicious frequencies
        float ultra18k = goertzel(samples, 1024, 18000, rate);
        float ultra20k = goertzel(samples, 1024, 20000, rate);
        float ultra22k = goertzel(samples, 1024, 22000, rate);

        // Check for common RF interference patterns
        float rf_buzz = goertzel(samples, 1024, 1000, rate);   // Common GSM buzz
        float rf_gsm  = goertzel(samples, 1024, 217, rate);    // GSM frame rate interference

        free(samples);

        float anomaly = (level / (baselineLevel + 1.0f)) - 1.0f;
        if (anomaly > maxAnomaly) maxAnomaly = anomaly;

        bool ultraAlert = (ultra18k > 2000 || ultra20k > 2000 || ultra22k > 2000);
        bool rfAlert = (rf_gsm > 3000);
        bool levelAlert = (anomaly > 2.0f);

        // Visual bar
        int barWidth = min((int)(anomaly * 30), tft.width() - 10);
        tft.fillRect(5, 80, tft.width() - 10, 20, bruceConfig.bgColor);
        uint16_t barColor = (anomaly < 1.0f) ? TFT_GREEN : (anomaly < 2.0f) ? TFT_YELLOW : TFT_RED;
        if (barWidth > 0) tft.fillRect(5, 80, barWidth, 20, barColor);

        tft.fillRect(0, 105, tft.width(), 70, bruceConfig.bgColor);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
        tft.drawString("Level: " + String(level, 0) + " (" + String(anomaly, 1) + "x)", 5, 105, 1);
        tft.drawString("Ultra: " + String(ultra18k, 0) + "/" + String(ultra20k, 0) + "/" + String(ultra22k, 0), 5, 120, 1);
        tft.drawString("RF: GSM=" + String(rf_gsm, 0) + " Buzz=" + String(rf_buzz, 0), 5, 135, 1);

        if (ultraAlert || rfAlert || levelAlert) {
            alertCount++;
            ledFeedbackFlash(CRGB::Red, 200);
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            if (ultraAlert) tft.drawCentreString("!! ULTRASONIC DETECTED !!", tft.width() / 2, 155, 1);
            else if (rfAlert) tft.drawCentreString("!! RF INTERFERENCE !!", tft.width() / 2, 155, 1);
            else tft.drawCentreString("!! ANOMALY DETECTED !!", tft.width() / 2, 155, 1);
        }

        tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
        tft.drawString("Alerts: " + String(alertCount), 5, tft.height() - 15, 1);

        delay(50);
    }

    ledFeedbackRestore();
}

// ── Ultrasonic Jammer ──────────────────────────────────────────────────────────
// Emit inaudible high-frequency tones to disrupt nearby microphones

void ultrasonicJammer() {
    ledFeedbackSetMode(LED_FB_WIFI_ATTACK);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_MAGENTA, bruceConfig.bgColor);
    tft.drawCentreString("ULTRASONIC JAMMER", tft.width() / 2, 5, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("Jamming nearby mics", tft.width() / 2, 25, 1);
    tft.drawCentreString("[ESC] to stop", tft.width() / 2, tft.height() - 15, 1);

    // Frequencies that disrupt microphone AGC and voice recording
    const int jamFreqs[] = {18500, 19000, 19500, 20000, 20500, 21000, 21500, 22000};
    int freqCount = sizeof(jamFreqs) / sizeof(jamFreqs[0]);
    int freqIdx = 0;

    while (1) {
        if (check(EscPress)) break;

        // Rapidly sweep through ultrasonic frequencies
        playTone(jamFreqs[freqIdx], 20, 0);
        freqIdx = (freqIdx + 1) % freqCount;

        // Show activity
        if (freqIdx == 0) {
            tft.fillRect(0, 50, tft.width(), 30, bruceConfig.bgColor);
            tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
            tft.drawCentreString("Sweeping 18.5-22 kHz", tft.width() / 2, 55, 1);
        }
    }

    ledFeedbackRestore();
}

// ── Main Menu ──────────────────────────────────────────────────────────────────

void dtmfToolsMenu() {
    options = {
        {"DTMF Decode",      dtmfDecoder     },
        {"DTMF Encode",      dtmfEncoder     },
        {"DTMF Dial",        dtmfDialer      },
        {"Bug Detector",     audioBugDetector },
        {"Ultrasonic Jam",   ultrasonicJammer },
    };
    addOptionToMainMenu();
    loopOptions(options);
}

#endif // MIC + SPEAKER
