#include "rf_emulation.h"
#include "rf_bruteforce.h"
#include "rf_utils.h"
#include "core/display.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include <globals.h>

// ── Extended Brute Force Protocols ──────────────────────────────────────────
// More protocols beyond the base 6 in rf_bruteforce.h

static constexpr BruteProtocol extended_protocols[] = {
    // name                    bits  zero           one           pilot            stop
    {"CAME 12bit",            12, {-320, 640},  {-640, 320},  {-11520, 320}, {0, 0}       },
    {"Nice 12bit",            12, {-700, 1400}, {-1400, 700}, {-25200, 700}, {0, 0}       },
    {"Ansonic 12bit",         12, {-1111, 555}, {-555, 1111}, {-19425, 555}, {0, 0}       },
    {"Holtek 12bit",          12, {-870, 430},  {-430, 870},  {-15480, 430}, {0, 0}       },
    {"Linear 10bit",          10, {500, -1500}, {1500, -500}, {0, 0},        {500, -21500}},
    {"Chamberlain 9bit",       9, {-870, 430},  {-430, 870},  {0, 0},        {-3000, 1000}},
    // Extended protocols
    {"Princeton 24bit",       24, {350, -1050}, {1050, -350}, {350, -10850}, {0, 0}       },
    {"SMC5326 8bit",           8, {320, -960},  {960, -320},  {320, -11200}, {0, 0}       },
    {"PT2260 24bit",          24, {350, -1050}, {1050, -350}, {350, -10850}, {0, 0}       },
    {"Hormann HSM 44bit",     44, {500, -1000}, {1000, -500}, {0, 0},        {500, -15000}},
    {"Doorhan 16bit",         16, {380, -1140}, {1140, -380}, {380, -13300}, {0, 0}       },
    {"Beninca 12bit",         12, {320, -640},  {640, -320},  {320, -11520}, {0, 0}       },
    {"Elmes 18bit",           18, {320, -960},  {960, -320},  {320, -11200}, {0, 0}       },
    {"Motorline 8bit",         8, {400, -800},  {800, -400},  {400, -10000}, {0, 0}       },
    {"GateTX 24bit",          24, {380, -1140}, {1140, -380}, {380, -13300}, {0, 0}       },
    {"BFT Mitto 12bit",       12, {500, -1000}, {1000, -500}, {500, -15000}, {0, 0}       },
    {"Genius 12bit",          12, {340, -680},  {680, -340},  {340, -12240}, {0, 0}       },
    {"Alutech AT-4 16bit",    16, {400, -800},  {800, -400},  {400, -11200}, {0, 0}       },
};

static constexpr int EXTENDED_PROTOCOL_COUNT =
    sizeof(extended_protocols) / sizeof(extended_protocols[0]);

// ── Common device databases ─────────────────────────────────────────────────

struct DeviceTemplate {
    const char *name;
    float frequency;
    int protocol;   // RCSwitch protocol number
    int bits;
    int te;          // pulse length
    uint32_t code;   // example/default code (0 = iterate)
};

static const DeviceTemplate garageDoorTemplates[] = {
    {"CAME 433MHz", 433.92, 12, 12, 320, 0},
    {"Nice 433MHz", 433.92, 1, 12, 700, 0},
    {"Linear 300MHz", 300.00, 6, 10, 500, 0},
    {"Chamberlain 300MHz", 300.00, 8, 9, 430, 0},
    {"Chamberlain 315MHz", 315.00, 8, 9, 430, 0},
    {"Chamberlain 390MHz", 390.00, 8, 9, 430, 0},
    {"Liftmaster 315MHz", 315.00, 6, 10, 500, 0},
    {"Genie 390MHz", 390.00, 1, 12, 350, 0},
    {"Doorhan 433MHz", 433.92, 1, 16, 380, 0},
    {"BFT 433MHz", 433.92, 1, 12, 500, 0},
    {"Hormann 868MHz", 868.35, 1, 44, 500, 0},
    {"Marantec 868MHz", 868.35, 1, 12, 400, 0},
};
static constexpr int GARAGE_TEMPLATE_COUNT =
    sizeof(garageDoorTemplates) / sizeof(garageDoorTemplates[0]);

static const DeviceTemplate doorbellTemplates[] = {
    {"Generic 433MHz", 433.92, 1, 24, 350, 0},
    {"Byron 433MHz", 433.92, 1, 24, 320, 0},
    {"Friedland 433MHz", 433.92, 1, 24, 350, 0},
    {"Honeywell 315MHz", 315.00, 1, 24, 350, 0},
    {"SadoTech 315MHz", 315.00, 1, 24, 320, 0},
    {"Ring Chime 433MHz", 433.92, 1, 24, 350, 0},
};
static constexpr int DOORBELL_TEMPLATE_COUNT =
    sizeof(doorbellTemplates) / sizeof(doorbellTemplates[0]);

// ── Helper: send via CC1101 ─────────────────────────────────────────────────

static void sendPulse(int txpin, int duration) {
    if (duration > 0) {
        digitalWrite(txpin, HIGH);
        delayMicroseconds(duration);
    } else if (duration < 0) {
        digitalWrite(txpin, LOW);
        delayMicroseconds(-duration);
    }
}

static bool sendBruteCode(const BruteProtocol &proto, int code, int txpin, int repeats) {
    for (int r = 0; r < repeats; r++) {
        if (proto.pilot[0] || proto.pilot[1]) {
            sendPulse(txpin, proto.pilot[0]);
            sendPulse(txpin, proto.pilot[1]);
        }
        for (int j = proto.bits - 1; j >= 0; j--) {
            const int *timings = ((code >> j) & 1) ? proto.one : proto.zero;
            sendPulse(txpin, timings[0]);
            sendPulse(txpin, timings[1]);
        }
        if (proto.stop[0] || proto.stop[1]) {
            sendPulse(txpin, proto.stop[0]);
            sendPulse(txpin, proto.stop[1]);
        }
    }
    return true;
}

// ── Replay signal ───────────────────────────────────────────────────────────

void rfReplaySignal(float freq, uint64_t key, int protocol, int te, int repeat) {
    if (!initRfModule("tx", freq)) return;

    RCSwitch rcswitch;
    rcswitch.setProtocol(protocol);
    rcswitch.setPulseLength(te);
    rcswitch.setRepeatTransmit(repeat);

    int txpin = (bruceConfigPins.rfModule == CC1101_SPI_MODULE) ? bruceConfigPins.CC1101_bus.io0
                                                                : bruceConfigPins.rfTx;
    rcswitch.enableTransmit(txpin);

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Replaying Signal", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString(
        String(freq, 2) + "MHz  Code: 0x" + String((uint32_t)key, HEX), tftWidth / 2, 30, 1
    );

    for (int i = 0; i < repeat; i++) {
        if (check(EscPress)) break;
        rcswitch.send((uint32_t)key, 24);
        displayRedStripe(
            "TX " + String(i + 1) + "/" + String(repeat),
            getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
        );
        delay(100);
    }

    deinitRfModule();
    displaySuccess("Replay complete");
    delay(1000);
}

// ── Garage Door Emulator ────────────────────────────────────────────────────

void rfGarageDoorEmulator() {
    while (true) {
        int opt = 0;
        options = {};
        for (int i = 0; i < GARAGE_TEMPLATE_COUNT; i++) {
            int idx = i;
            options.push_back(
                {String(garageDoorTemplates[i].name).c_str(), [&opt, idx]() { opt = idx + 1; }}
            );
        }
        options.push_back({"Back", [&opt]() { opt = -1; }});
        loopOptions(options);

        if (opt <= 0) return;
        opt--; // Convert to 0-based index

        const DeviceTemplate &tmpl = garageDoorTemplates[opt];

        // Sub-menu: send single code or brute force
        int action = 0;
        options = {
            {"Brute Force All", [&]() { action = 1; }},
            {"Send Test Code", [&]() { action = 2; }},
            {"Back", [&]() { action = 0; }},
        };
        loopOptions(options);

        if (action == 1) {
            // Find matching protocol in extended list
            int protoIdx = -1;
            for (int i = 0; i < EXTENDED_PROTOCOL_COUNT; i++) {
                if (extended_protocols[i].bits == tmpl.bits) {
                    protoIdx = i;
                    break;
                }
            }
            if (protoIdx < 0) {
                displayError("No matching protocol");
                delay(1000);
                continue;
            }

            int txpin = (bruceConfigPins.rfModule == CC1101_SPI_MODULE)
                            ? bruceConfigPins.CC1101_bus.io0
                            : bruceConfigPins.rfTx;
            if (!initRfModule("tx", tmpl.frequency)) continue;

            pinMode(txpin, OUTPUT);
            setMHZ(tmpl.frequency);

            const BruteProtocol &proto = extended_protocols[protoIdx];
            int total = 1 << proto.bits;

            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString(tmpl.name, tftWidth / 2, 5, 2);

            for (int code = 0; code < total; code++) {
                if (check(EscPress)) break;

                sendBruteCode(proto, code, txpin, 2);

                if (code % 20 == 0) {
                    displayRedStripe(
                        String(code) + "/" + String(total) + " " + tmpl.name,
                        getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
                    );
                }
            }

            digitalWrite(txpin, LOW);
            deinitRfModule();
        } else if (action == 2) {
            rfReplaySignal(tmpl.frequency, 0xABCDEF, tmpl.protocol, tmpl.te, 10);
        }
    }
}

// ── Doorbell Emulator ───────────────────────────────────────────────────────

void rfDoorbellEmulator() {
    while (true) {
        int opt = 0;
        options = {};
        for (int i = 0; i < DOORBELL_TEMPLATE_COUNT; i++) {
            int idx = i;
            options.push_back(
                {String(doorbellTemplates[i].name).c_str(), [&opt, idx]() { opt = idx + 1; }}
            );
        }
        options.push_back({"Back", [&opt]() { opt = -1; }});
        loopOptions(options);

        if (opt <= 0) return;
        opt--;

        const DeviceTemplate &tmpl = doorbellTemplates[opt];

        // Brute force doorbell codes - 24 bit space is large so offer ranges
        int action = 0;
        options = {
            {"Brute 0x000-0xFFF", [&]() { action = 1; }},
            {"Brute 0xFFF-0x1FFF", [&]() { action = 2; }},
            {"Ring Test", [&]() { action = 3; }},
            {"Back", [&]() { action = 0; }},
        };
        loopOptions(options);

        if (action == 0) continue;

        if (!initRfModule("tx", tmpl.frequency)) continue;

        RCSwitch rcswitch;
        int txpin = (bruceConfigPins.rfModule == CC1101_SPI_MODULE)
                        ? bruceConfigPins.CC1101_bus.io0
                        : bruceConfigPins.rfTx;
        rcswitch.enableTransmit(txpin);
        rcswitch.setProtocol(tmpl.protocol);
        rcswitch.setPulseLength(tmpl.te);
        rcswitch.setRepeatTransmit(3);

        uint32_t start = 0, end = 0;
        if (action == 1) { start = 0x000; end = 0xFFF; }
        else if (action == 2) { start = 0xFFF; end = 0x1FFF; }
        else if (action == 3) { start = 0x555555; end = 0x555555; }

        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Doorbell: " + String(tmpl.name), tftWidth / 2, 5, 2);

        uint32_t total = end - start + 1;
        uint32_t count = 0;

        for (uint32_t code = start; code <= end; code++) {
            if (check(EscPress)) break;
            count++;

            rcswitch.send(code, tmpl.bits);

            if (count % 10 == 0) {
                displayRedStripe(
                    "0x" + String(code, HEX) + " (" + String(count * 100 / total) + "%)",
                    getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
                );
            }
            delay(50);
        }

        deinitRfModule();
        displaySuccess("Done");
        delay(1000);
    }
}

// ── Signal Analyzer ─────────────────────────────────────────────────────────

void rfSignalAnalyzer() {
    if (!initRfModule("rx", bruceConfigPins.rfFreq)) return;

    RCSwitch rcswitch;
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
        rcswitch.enableReceive(bruceConfigPins.CC1101_bus.io0);
    } else {
        rcswitch.enableReceive(bruceConfigPins.rfRx);
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Signal Analyzer", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString(
        String(bruceConfigPins.rfFreq, 2) + " MHz - Waiting...", tftWidth / 2, 25, 1
    );

    int signalCount = 0;

    while (!check(EscPress)) {
        if (rcswitch.available()) {
            signalCount++;
            uint32_t value = rcswitch.getReceivedValue();
            int bits = rcswitch.getReceivedBitlength();
            int proto = rcswitch.getReceivedProtocol();
            int te = rcswitch.getReceivedDelay();

            tft.fillRect(0, 40, tftWidth, tftHeight - 40, bruceConfig.bgColor);
            int y = 42;

            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawString("Signal #" + String(signalCount), 10, y, 2);
            y += 22;

            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawString("Value: 0x" + String(value, HEX), 10, y, 1);
            y += 16;
            tft.drawString("Binary: " + String(value, BIN), 10, y, 1);
            y += 16;
            tft.drawString("Bits: " + String(bits), 10, y, 1);
            y += 16;
            tft.drawString("Protocol: " + String(proto), 10, y, 1);
            y += 16;
            tft.drawString("Pulse (TE): " + String(te) + "us", 10, y, 1);
            y += 16;

            // Estimate what this could be
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            String guess = "Unknown";
            if (bits == 12) guess = "Likely: Gate/Garage (CAME/Nice)";
            else if (bits == 24) guess = "Likely: Doorbell/Switch (Princeton)";
            else if (bits == 10) guess = "Likely: Linear garage";
            else if (bits == 9) guess = "Likely: Chamberlain";
            else if (bits >= 32) guess = "Likely: Rolling code / KeeLoq";
            tft.drawString(guess, 10, y, 1);

            rcswitch.resetAvailable();
        }
        delay(10);
    }

    deinitRfModule();
}

// ── Enhanced Brute Force Menu ───────────────────────────────────────────────

void rfEnhancedBruteForce() {
    static float bruteFreq = 433.92;
    static int protoIdx = 0;
    static int repeats = 2;

    while (true) {
        int opt = 0;
        options = {
            {"Frequency: " + String(bruteFreq, 2) + "MHz", [&]() { opt = 1; }},
            {String("Protocol: ") + extended_protocols[protoIdx].name, [&]() { opt = 2; }},
            {"Repeats: " + String(repeats), [&]() { opt = 3; }},
            {"Start", [&]() { opt = 4; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: {
                options = {};
                int ind = 0;
                int arraySize = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
                for (int i = 0; i < arraySize; i++) {
                    String tmp = String(subghz_frequency_list[i], 2) + "MHz";
                    float f = subghz_frequency_list[i];
                    options.push_back({tmp.c_str(), [&bruteFreq, f]() { bruteFreq = f; }});
                    if (int(bruteFreq * 100) == int(subghz_frequency_list[i] * 100)) ind = i;
                }
                loopOptions(options, ind);
                break;
            }
            case 2: {
                options = {};
                for (int i = 0; i < EXTENDED_PROTOCOL_COUNT; i++) {
                    int idx = i;
                    options.push_back(
                        {extended_protocols[i].name, [&protoIdx, idx]() { protoIdx = idx; }}
                    );
                }
                loopOptions(options, protoIdx);
                break;
            }
            case 3: {
                options = {};
                for (int i = 1; i <= 5; i++) {
                    options.push_back({String(i).c_str(), [&repeats, i]() { repeats = i; }});
                }
                loopOptions(options, repeats - 1);
                break;
            }
            case 4: {
                int txpin = (bruceConfigPins.rfModule == CC1101_SPI_MODULE)
                                ? bruceConfigPins.CC1101_bus.io0
                                : bruceConfigPins.rfTx;
                if (!initRfModule("tx", bruteFreq)) break;

                pinMode(txpin, OUTPUT);
                setMHZ(bruteFreq);

                const BruteProtocol &proto = extended_protocols[protoIdx];
                int total = 1 << proto.bits;

                for (int code = 0; code < total; code++) {
                    if (check(EscPress)) break;

                    sendBruteCode(proto, code, txpin, repeats);

                    if (code % 10 == 0) {
                        displayRedStripe(
                            String(code) + "/" + String(total) + " " + proto.name,
                            getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
                        );
                    }
                }

                digitalWrite(txpin, LOW);
                deinitRfModule();
                break;
            }
            default: return;
        }
    }
}

// ── Gate Emulator (same structure as garage) ────────────────────────────────

void rfGateEmulator() { rfGarageDoorEmulator(); } // Shares templates

// ── Key Fob Emulator (fixed codes only) ─────────────────────────────────────

void rfKeyFobEmulator() {
    displayRedStripe("Fixed-code fobs only");
    delay(1500);
    rfGarageDoorEmulator();
}

// ── Main RF Emulation Menu ──────────────────────────────────────────────────

void rfEmulationMenu() {
    while (true) {
        int opt = 0;
        options = {
            {"Garage Doors", [&]() { opt = 1; }},
            {"Doorbells", [&]() { opt = 2; }},
            {"Signal Analyzer", [&]() { opt = 3; }},
            {"Extended Brute Force", [&]() { opt = 4; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: rfGarageDoorEmulator(); break;
            case 2: rfDoorbellEmulator(); break;
            case 3: rfSignalAnalyzer(); break;
            case 4: rfEnhancedBruteForce(); break;
            default: return;
        }
    }
}
