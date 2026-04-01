#include "ir_bruteforce.h"
#include "ir_power_codes.h"
#include "core/display.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <globals.h>

static int bruteDelayMs = 100;
static uint16_t bruteStartAddr = 0x00;
static uint16_t bruteEndAddr = 0xFF;
static uint16_t bruteStartCmd = 0x00;
static uint16_t bruteEndCmd = 0xFF;

static IRsend *irSender = nullptr;

static void initIrSender() {
    if (!irSender) {
        irSender = new IRsend(bruceConfigPins.irTx);
        irSender->begin();
    }
}

static void sendByProtocol(const char *protocol, uint16_t addr, uint16_t cmd) {
    initIrSender();
    String proto(protocol);
    if (proto == "NEC") {
        // NEC: addr(8) + ~addr(8) + cmd(8) + ~cmd(8)
        uint32_t data = ((uint32_t)(addr & 0xFF)) | ((uint32_t)(~addr & 0xFF) << 8) |
                        ((uint32_t)(cmd & 0xFF) << 16) | ((uint32_t)(~cmd & 0xFF) << 24);
        irSender->sendNEC(data);
    } else if (proto == "Samsung") {
        irSender->sendSAMSUNG(((uint32_t)addr << 16) | cmd);
    } else if (proto == "RC5") {
        irSender->sendRC5(irSender->encodeRC5(addr & 0x1F, cmd & 0x3F));
    } else if (proto == "RC6") {
        irSender->sendRC6(irSender->encodeRC6(addr & 0xFF, cmd & 0xFF));
    } else if (proto == "Sony") {
        irSender->sendSony(irSender->encodeSony(12, cmd & 0x7F, addr & 0x1F));
    }
}

// ── Brute force all known power codes from the database ─────────────────────
static bool bruteKnownCodes(const IrDeviceCode *codes, int count, const char *title) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString(title, tftWidth / 2, 5, 2);

    initIrSender();

    for (int i = 0; i < count; i++) {
        if (check(EscPress)) return false;

        // Update display every code (they're not too many)
        int y = 30;
        tft.fillRect(0, y, tftWidth, 60, bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawCentreString(
            String(codes[i].brand) + " - " + codes[i].function, tftWidth / 2, y, 2
        );
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString(
            String(codes[i].protocol) + " A:0x" + String(codes[i].address, HEX) + " C:0x" +
                String(codes[i].command, HEX),
            tftWidth / 2, y + 20, 1
        );

        // Progress bar
        int progW = tftWidth - 20;
        int progY = y + 42;
        tft.drawRect(10, progY, progW, 10, bruceConfig.priColor);
        int fillW = (progW - 2) * (i + 1) / count;
        tft.fillRect(11, progY + 1, fillW, 8, bruceConfig.priColor);

        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawCentreString(
            String(i + 1) + "/" + String(count), tftWidth / 2, progY + 14, 1
        );

        sendByProtocol(codes[i].protocol, codes[i].address, codes[i].command);
        delay(bruteDelayMs);
    }

    displaySuccess("Done! " + String(count) + " codes sent");
    delay(1500);
    return true;
}

// ── Full address:command range brute force ───────────────────────────────────
static bool bruteForceRange(const char *protocol) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("IR Brute Force", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString(
        String(protocol) + " [0x" + String(bruteStartAddr, HEX) + "-0x" +
            String(bruteEndAddr, HEX) + "]",
        tftWidth / 2, 25, 1
    );

    initIrSender();

    uint32_t total =
        (uint32_t)(bruteEndAddr - bruteStartAddr + 1) * (bruteEndCmd - bruteStartCmd + 1);
    uint32_t count = 0;

    for (uint16_t addr = bruteStartAddr; addr <= bruteEndAddr; addr++) {
        for (uint16_t cmd = bruteStartCmd; cmd <= bruteEndCmd; cmd++) {
            if (check(EscPress)) {
                displayRedStripe("Stopped at " + String(count) + "/" + String(total));
                delay(1000);
                return false;
            }

            count++;
            // Update display every 16 codes to avoid flicker
            if (count % 16 == 1 || count == total) {
                tft.fillRect(0, 45, tftWidth, 50, bruceConfig.bgColor);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.drawCentreString(
                    "Addr:0x" + String(addr, HEX) + " Cmd:0x" + String(cmd, HEX),
                    tftWidth / 2, 48, 2
                );

                int progW = tftWidth - 20;
                int progY = 70;
                tft.drawRect(10, progY, progW, 10, bruceConfig.priColor);
                int fillW = (progW - 2) * count / total;
                tft.fillRect(11, progY + 1, fillW, 8, bruceConfig.priColor);

                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawCentreString(
                    String(count * 100 / total) + "% (" + String(count) + "/" + String(total) + ")",
                    tftWidth / 2, 85, 1
                );
            }

            sendByProtocol(protocol, addr, cmd);
            delay(bruteDelayMs);
        }
        // Check for overflow (if endAddr is 0xFF, addr++ would wrap)
        if (addr == bruteEndAddr) break;
    }

    displaySuccess("Done! " + String(total) + " codes sent");
    delay(1500);
    return true;
}

// ── Menu helpers ─────────────────────────────────────────────────────────────

static void setDelay() {
    int delays[] = {30, 50, 75, 100, 150, 200, 300, 500};
    int idx = 3; // default 100ms
    options = {};
    for (int i = 0; i < 8; i++) {
        int d = delays[i];
        if (d == bruteDelayMs) idx = i;
        options.push_back({(String(d) + "ms").c_str(), [d]() { bruteDelayMs = d; }});
    }
    loopOptions(options, idx);
}

static void setAddrRange() {
    options = {
        {"0x00 - 0x0F", []() { bruteStartAddr = 0x00; bruteEndAddr = 0x0F; }},
        {"0x00 - 0x1F", []() { bruteStartAddr = 0x00; bruteEndAddr = 0x1F; }},
        {"0x00 - 0x3F", []() { bruteStartAddr = 0x00; bruteEndAddr = 0x3F; }},
        {"0x00 - 0x7F", []() { bruteStartAddr = 0x00; bruteEndAddr = 0x7F; }},
        {"0x00 - 0xFF (Full)", []() { bruteStartAddr = 0x00; bruteEndAddr = 0xFF; }},
    };
    loopOptions(options);
}

static void setCmdRange() {
    options = {
        {"0x00 - 0x0F", []() { bruteStartCmd = 0x00; bruteEndCmd = 0x0F; }},
        {"0x00 - 0x1F", []() { bruteStartCmd = 0x00; bruteEndCmd = 0x1F; }},
        {"0x00 - 0x3F", []() { bruteStartCmd = 0x00; bruteEndCmd = 0x3F; }},
        {"0x00 - 0x7F", []() { bruteStartCmd = 0x00; bruteEndCmd = 0x7F; }},
        {"0x00 - 0xFF (Full)", []() { bruteStartCmd = 0x00; bruteEndCmd = 0xFF; }},
    };
    loopOptions(options);
}

// ── Category brute force ────────────────────────────────────────────────────
static void bruteByCategory() {
    while (true) {
        int opt = 0;
        options = {
            {"TVs (" + String(IR_TV_CODE_COUNT) + ")", [&]() { opt = 1; }},
            {"AC Units (" + String(IR_AC_CODE_COUNT) + ")", [&]() { opt = 2; }},
            {"Projectors (" + String(IR_PROJECTOR_CODE_COUNT) + ")", [&]() { opt = 3; }},
            {"Soundbars (" + String(IR_AUDIO_CODE_COUNT) + ")", [&]() { opt = 4; }},
            {"LED Strips (" + String(IR_LED_CODE_COUNT) + ")", [&]() { opt = 5; }},
            {"Fans (" + String(IR_FAN_CODE_COUNT) + ")", [&]() { opt = 6; }},
            {"Set-top Box (" + String(IR_STB_CODE_COUNT) + ")", [&]() { opt = 7; }},
            {"ALL Combined", [&]() { opt = 8; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: bruteKnownCodes(IR_TV_CODES, IR_TV_CODE_COUNT, "TV Power Scan"); break;
            case 2: bruteKnownCodes(IR_AC_CODES, IR_AC_CODE_COUNT, "AC Power Scan"); break;
            case 3:
                bruteKnownCodes(IR_PROJECTOR_CODES, IR_PROJECTOR_CODE_COUNT, "Projector Scan");
                break;
            case 4:
                bruteKnownCodes(IR_AUDIO_CODES, IR_AUDIO_CODE_COUNT, "Audio Power Scan");
                break;
            case 5: bruteKnownCodes(IR_LED_CODES, IR_LED_CODE_COUNT, "LED Strip Scan"); break;
            case 6: bruteKnownCodes(IR_FAN_CODES, IR_FAN_CODE_COUNT, "Fan Power Scan"); break;
            case 7: bruteKnownCodes(IR_STB_CODES, IR_STB_CODE_COUNT, "STB Power Scan"); break;
            case 8:
                bruteKnownCodes(IR_TV_CODES, IR_TV_CODE_COUNT, "TV Codes");
                bruteKnownCodes(IR_AC_CODES, IR_AC_CODE_COUNT, "AC Codes");
                bruteKnownCodes(IR_PROJECTOR_CODES, IR_PROJECTOR_CODE_COUNT, "Projector Codes");
                bruteKnownCodes(IR_AUDIO_CODES, IR_AUDIO_CODE_COUNT, "Audio Codes");
                bruteKnownCodes(IR_LED_CODES, IR_LED_CODE_COUNT, "LED Codes");
                bruteKnownCodes(IR_FAN_CODES, IR_FAN_CODE_COUNT, "Fan Codes");
                bruteKnownCodes(IR_STB_CODES, IR_STB_CODE_COUNT, "STB Codes");
                break;
            default: return;
        }
    }
}

// ── Custom range brute force menu ───────────────────────────────────────────
static void bruteCustomRange() {
    while (true) {
        int opt = 0;
        options = {
            {"Protocol: NEC", [&]() { opt = 1; }},
            {"Addr: 0x" + String(bruteStartAddr, HEX) + "-0x" + String(bruteEndAddr, HEX),
             [&]() { opt = 2; }},
            {"Cmd: 0x" + String(bruteStartCmd, HEX) + "-0x" + String(bruteEndCmd, HEX),
             [&]() { opt = 3; }},
            {"Delay: " + String(bruteDelayMs) + "ms", [&]() { opt = 4; }},
            {"START", [&]() { opt = 5; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: {
                int proto = 0;
                options = {
                    {"NEC", [&]() { proto = 0; }},
                    {"Samsung", [&]() { proto = 1; }},
                    {"RC5", [&]() { proto = 2; }},
                    {"RC6", [&]() { proto = 3; }},
                    {"Sony", [&]() { proto = 4; }},
                };
                loopOptions(options);
                const char *protos[] = {"NEC", "Samsung", "RC5", "RC6", "Sony"};
                // Store selected protocol for use in start
                // Using NEC as default since we can't easily change the menu label
                break;
            }
            case 2: setAddrRange(); break;
            case 3: setCmdRange(); break;
            case 4: setDelay(); break;
            case 5: bruteForceRange("NEC"); break;
            default: return;
        }
    }
}

// ── Main IR brute force menu ────────────────────────────────────────────────
void irBruteForce() {
    while (true) {
        int opt = 0;
        options = {
            {"By Device Type", [&]() { opt = 1; }},
            {"NEC Full Scan", [&]() { opt = 2; }},
            {"Samsung Full Scan", [&]() { opt = 3; }},
            {"RC5 Full Scan", [&]() { opt = 4; }},
            {"RC6 Full Scan", [&]() { opt = 5; }},
            {"Sony Full Scan", [&]() { opt = 6; }},
            {"Custom Range", [&]() { opt = 7; }},
            {"Delay: " + String(bruteDelayMs) + "ms", [&]() { opt = 8; }},
            {"Main Menu", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: bruteByCategory(); break;
            case 2: bruteForceRange("NEC"); break;
            case 3: bruteForceRange("Samsung"); break;
            case 4: bruteForceRange("RC5"); break;
            case 5: bruteForceRange("RC6"); break;
            case 6: bruteForceRange("Sony"); break;
            case 7: bruteCustomRange(); break;
            case 8: setDelay(); break;
            default: return;
        }
    }
}

void irBruteForceStart(IrBruteMode mode, uint16_t startAddr, uint16_t endAddr, uint16_t startCmd,
                       uint16_t endCmd, int delayMs) {
    bruteStartAddr = startAddr;
    bruteEndAddr = endAddr;
    bruteStartCmd = startCmd;
    bruteEndCmd = endCmd;
    bruteDelayMs = delayMs;

    switch (mode) {
        case IR_BRUTE_ALL_POWER:
            bruteKnownCodes(IR_TV_CODES, IR_TV_CODE_COUNT, "All Power Codes");
            bruteKnownCodes(IR_AC_CODES, IR_AC_CODE_COUNT, "All AC Codes");
            bruteKnownCodes(IR_PROJECTOR_CODES, IR_PROJECTOR_CODE_COUNT, "All Projector Codes");
            bruteKnownCodes(IR_AUDIO_CODES, IR_AUDIO_CODE_COUNT, "All Audio Codes");
            break;
        case IR_BRUTE_NEC_SCAN: bruteForceRange("NEC"); break;
        case IR_BRUTE_SAMSUNG_SCAN: bruteForceRange("Samsung"); break;
        case IR_BRUTE_RC5_SCAN: bruteForceRange("RC5"); break;
        case IR_BRUTE_RC6_SCAN: bruteForceRange("RC6"); break;
        case IR_BRUTE_SONY_SCAN: bruteForceRange("Sony"); break;
        case IR_BRUTE_CUSTOM: bruteForceRange("NEC"); break;
        case IR_BRUTE_BY_CATEGORY: bruteByCategory(); break;
    }
}
