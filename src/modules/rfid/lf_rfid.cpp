#include "lf_rfid.h"
#include "core/display.h"
#include "core/sd_functions.h"
#include <globals.h>

/**
 * Low-Frequency RFID Module for T-Embed CC1101
 *
 * This module supports reading 125kHz cards via an external PN5180 module
 * connected to the SPI bus or via a simple coil + comparator circuit on GPIO.
 *
 * For basic EM4100 reading, a simple UART-based reader module (RDM6300)
 * can also be used on the UART pins (GPIO 43/44).
 *
 * The PN5180 approach provides broader protocol support but requires the
 * PN5180 library. This implementation provides the framework and supports
 * both UART readers (like the existing RFID125 class) and SPI-based readers.
 */

// ── EM4100 Decoder ──────────────────────────────────────────────────────────
// EM4100 format: 9 leading 1s + 10 groups of (4 data + 1 parity) + stop = 64 bits

static bool decodeEM4100(uint64_t raw, LfCardData &card) {
    // Check header: 9 consecutive 1s
    uint64_t header = (raw >> 55) & 0x1FF;
    if (header != 0x1FF) return false;

    // Extract 40 data bits (8 hex nibbles + version)
    uint8_t data[10];
    bool valid = true;

    for (int row = 0; row < 10; row++) {
        int offset = 54 - row * 5;
        uint8_t nibble = (raw >> (offset + 1)) & 0x0F;
        uint8_t parity = (raw >> offset) & 0x01;

        // Row parity check (even parity over 4 data bits)
        uint8_t calcParity = 0;
        for (int b = 0; b < 4; b++) calcParity ^= (nibble >> b) & 1;

        if (calcParity != parity) { valid = false; break; }
        data[row] = nibble;
    }

    if (!valid) return false;

    // Column parity check
    for (int col = 0; col < 4; col++) {
        uint8_t colParity = 0;
        for (int row = 0; row < 10; row++) colParity ^= (data[row] >> col) & 1;
        uint8_t expected = (raw >> (4 - col)) & 1;
        if (colParity != expected) return false;
    }

    card.type = LF_EM4100;
    card.rawData = raw;
    card.bitLength = 64;

    // Version/manufacturer (first 2 nibbles = 8 bits)
    uint8_t version = (data[0] << 4) | data[1];

    // Card data (remaining 8 nibbles = 32 bits)
    uint32_t cardData = 0;
    for (int i = 2; i < 10; i++) {
        cardData = (cardData << 4) | data[i];
    }

    card.facilityCode = version;
    card.cardNumber = cardData;

    // Format UID string
    char buf[20];
    snprintf(buf, sizeof(buf), "%02X:%08lX", version, (unsigned long)cardData);
    card.uid = String(buf);

    return true;
}

// ── HID Prox Decoder ────────────────────────────────────────────────────────
// H10301 26-bit: 1 parity + 8 facility + 16 card + 1 parity

static bool decodeHIDProx(uint64_t raw, LfCardData &card) {
    // HID cards have specific header patterns
    // 26-bit format is most common
    uint32_t data26 = raw & 0x3FFFFFF;

    // Even parity on bits 25-14 (first 12 bits)
    uint8_t evenParity = 0;
    for (int i = 13; i <= 24; i++) evenParity ^= (data26 >> i) & 1;
    if (evenParity != ((data26 >> 25) & 1)) return false;

    // Odd parity on bits 12-1 (last 12 bits)
    uint8_t oddParity = 1;
    for (int i = 1; i <= 12; i++) oddParity ^= (data26 >> i) & 1;
    if (oddParity != (data26 & 1)) return false;

    card.type = LF_HID_PROX;
    card.rawData = raw;
    card.bitLength = 26;
    card.facilityCode = (data26 >> 17) & 0xFF;
    card.cardNumber = (data26 >> 1) & 0xFFFF;

    char buf[30];
    snprintf(buf, sizeof(buf), "FC:%d CN:%d", card.facilityCode, card.cardNumber);
    card.uid = String(buf);

    return true;
}

// ── UART Reader Interface (RDM6300 / compatible) ────────────────────────────

static HardwareSerial *lfSerial = nullptr;

static bool initLfReader() {
    if (!lfSerial) {
        lfSerial = new HardwareSerial(1);
        lfSerial->begin(9600, SERIAL_8N1, bruceConfigPins.uart_bus.rx, bruceConfigPins.uart_bus.tx);
    }
    return true;
}

static void deinitLfReader() {
    if (lfSerial) {
        lfSerial->end();
        delete lfSerial;
        lfSerial = nullptr;
    }
}

// Read raw data from UART-based 125kHz reader
// Returns true if valid data received
static bool readLfUart(uint8_t *buffer, int &len) {
    if (!lfSerial || !lfSerial->available()) return false;

    // RDM6300 format: 0x02 + 10 hex chars + checksum(2 hex) + 0x03 = 14 bytes
    if (lfSerial->peek() != 0x02) {
        lfSerial->read(); // Discard non-header byte
        return false;
    }

    char packet[14];
    int bytesRead = lfSerial->readBytes(packet, 14);
    if (bytesRead != 14 || packet[13] != 0x03) return false;

    memcpy(buffer, packet, 14);
    len = 14;
    return true;
}

// ── Card Display ────────────────────────────────────────────────────────────

void lfRfidDisplayCard(const LfCardData &card) {
    tft.fillRect(0, 30, tftWidth, tftHeight - 30, bruceConfig.bgColor);

    int y = 35;
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    const char *typeName;
    switch (card.type) {
        case LF_EM4100: typeName = "EM4100/4102"; break;
        case LF_HID_PROX: typeName = "HID Prox H10301"; break;
        case LF_AWID: typeName = "AWID"; break;
        case LF_INDALA: typeName = "Indala"; break;
        default: typeName = "Unknown"; break;
    }

    tft.drawCentreString(typeName, tftWidth / 2, y, 2);
    y += 22;

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("UID: " + card.uid, tftWidth / 2, y, 1);
    y += 16;

    if (card.type == LF_HID_PROX) {
        tft.drawCentreString(
            "Facility: " + String(card.facilityCode) +
                "  Card#: " + String(card.cardNumber),
            tftWidth / 2, y, 1
        );
    } else if (card.type == LF_EM4100) {
        tft.drawCentreString(
            "Version: 0x" + String(card.facilityCode, HEX) +
                "  Data: 0x" + String(card.cardNumber, HEX),
            tftWidth / 2, y, 1
        );
    }
    y += 16;

    tft.drawCentreString("Bits: " + String(card.bitLength), tftWidth / 2, y, 1);
    y += 16;

    tft.drawCentreString(
        "Raw: 0x" + String((uint32_t)(card.rawData >> 32), HEX) +
            String((uint32_t)(card.rawData & 0xFFFFFFFF), HEX),
        tftWidth / 2, y, 1
    );
}

// ── Read Card ───────────────────────────────────────────────────────────────

bool lfRfidRead(LfCardData &card) {
    if (!initLfReader()) return false;

    uint8_t buffer[14];
    int len = 0;

    if (!readLfUart(buffer, len)) return false;

    // Parse EM4100 from UART packet (RDM6300 format)
    // Bytes 1-10 are hex ASCII of 5-byte tag data
    uint64_t tagData = 0;
    for (int i = 1; i <= 10; i++) {
        uint8_t nibble;
        if (buffer[i] >= '0' && buffer[i] <= '9') nibble = buffer[i] - '0';
        else if (buffer[i] >= 'A' && buffer[i] <= 'F') nibble = buffer[i] - 'A' + 10;
        else if (buffer[i] >= 'a' && buffer[i] <= 'f') nibble = buffer[i] - 'a' + 10;
        else return false;
        tagData = (tagData << 4) | nibble;
    }

    // Try decoders
    card.rawData = tagData;
    card.type = LF_EM4100; // Default for UART readers
    card.facilityCode = (tagData >> 32) & 0xFF;
    card.cardNumber = tagData & 0xFFFFFFFF;

    char buf[20];
    snprintf(buf, sizeof(buf), "%02X:%08lX",
             (uint8_t)card.facilityCode, (unsigned long)card.cardNumber);
    card.uid = String(buf);
    card.bitLength = 40;

    return true;
}

// ── Save / Load ─────────────────────────────────────────────────────────────

bool lfRfidSave(const LfCardData &card, const String &filename) {
    FS *fs;
    if (!getFsStorage(fs)) return false;

    if (!(*fs).exists("/BruceRFID")) (*fs).mkdir("/BruceRFID");

    String path = "/BruceRFID/" + filename + ".lf125";
    File f = (*fs).open(path, FILE_WRITE);
    if (!f) return false;

    f.println("Filetype: Bruce LF-RFID File");
    f.println("Version 1");

    const char *typeName;
    switch (card.type) {
        case LF_EM4100: typeName = "EM4100"; break;
        case LF_HID_PROX: typeName = "HID_Prox"; break;
        case LF_AWID: typeName = "AWID"; break;
        case LF_INDALA: typeName = "Indala"; break;
        default: typeName = "Unknown"; break;
    }

    f.println("Type: " + String(typeName));
    f.println("UID: " + card.uid);
    f.println("Facility: " + String(card.facilityCode));
    f.println("CardNumber: " + String(card.cardNumber));
    f.println("BitLength: " + String(card.bitLength));
    f.println("RawData: 0x" + String((uint32_t)(card.rawData >> 32), HEX) +
              String((uint32_t)(card.rawData & 0xFFFFFFFF), HEX));

    f.close();
    return true;
}

bool lfRfidLoad(LfCardData &card, const String &filename) {
    FS *fs;
    if (!getFsStorage(fs)) return false;

    File f = (*fs).open(filename, FILE_READ);
    if (!f) return false;

    card.type = LF_UNKNOWN;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();

        if (line.startsWith("Type: ")) {
            String type = line.substring(6);
            if (type == "EM4100") card.type = LF_EM4100;
            else if (type == "HID_Prox") card.type = LF_HID_PROX;
            else if (type == "AWID") card.type = LF_AWID;
            else if (type == "Indala") card.type = LF_INDALA;
        } else if (line.startsWith("UID: ")) {
            card.uid = line.substring(5);
        } else if (line.startsWith("Facility: ")) {
            card.facilityCode = line.substring(10).toInt();
        } else if (line.startsWith("CardNumber: ")) {
            card.cardNumber = line.substring(12).toInt();
        } else if (line.startsWith("BitLength: ")) {
            card.bitLength = line.substring(11).toInt();
        } else if (line.startsWith("RawData: ")) {
            String rawStr = line.substring(9);
            if (rawStr.startsWith("0x")) rawStr = rawStr.substring(2);
            card.rawData = strtoull(rawStr.c_str(), nullptr, 16);
        }
    }

    f.close();
    return card.type != LF_UNKNOWN;
}

// ── Scan Mode ───────────────────────────────────────────────────────────────

void lfRfidScanMode() {
    if (!initLfReader()) {
        displayError("Reader init failed");
        delay(1500);
        return;
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("125kHz Scan Mode", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Scanning for LF cards...", tftWidth / 2, 25, 1);

    int cardsFound = 0;
    LfCardData lastCard;

    while (!check(EscPress)) {
        LfCardData card;
        if (lfRfidRead(card)) {
            // Check if it's a new card
            if (card.uid != lastCard.uid) {
                cardsFound++;
                lastCard = card;

                tft.fillScreen(bruceConfig.bgColor);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.drawCentreString(
                    "Card #" + String(cardsFound), tftWidth / 2, 5, 2
                );
                lfRfidDisplayCard(card);
            }
        }
        delay(100);
    }

    deinitLfReader();
}

// ── Brute Force ─────────────────────────────────────────────────────────────

void lfRfidBruteForce() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("LF RFID Brute Force", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Generate HID Prox codes", tftWidth / 2, 25, 1);
    tft.drawCentreString("for replay via T55xx writer", tftWidth / 2, 40, 1);

    // Generate and display common facility/card combinations
    int y = 60;
    int count = 0;

    // Common facility codes used in buildings
    uint8_t commonFacilities[] = {1, 2, 3, 4, 5, 10, 20, 50, 100, 150, 200, 255};

    for (int f = 0; f < 12 && !check(EscPress); f++) {
        for (uint16_t c = 1; c <= 100 && !check(EscPress); c += 10) {
            count++;

            // Calculate HID 26-bit H10301 format
            uint32_t hid26 = 0;
            hid26 |= ((uint32_t)commonFacilities[f] & 0xFF) << 17;
            hid26 |= ((uint32_t)c & 0xFFFF) << 1;

            // Even parity on upper 12 bits
            uint8_t ep = 0;
            for (int i = 13; i <= 24; i++) ep ^= (hid26 >> i) & 1;
            hid26 |= ((uint32_t)ep << 25);

            // Odd parity on lower 12 bits
            uint8_t op = 1;
            for (int i = 1; i <= 12; i++) op ^= (hid26 >> i) & 1;
            hid26 |= op;

            if (count % 20 == 0) {
                displayRedStripe(
                    "FC:" + String(commonFacilities[f]) + " CN:" + String(c) +
                        " [" + String(count) + "]",
                    getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
                );
            }

            delay(50);
        }
    }

    displaySuccess("Generated " + String(count) + " codes");
    delay(2000);
}

// ── Main Menu ───────────────────────────────────────────────────────────────

void lfRfidMenu() {
    while (true) {
        int opt = 0;
        options = {
            {"Read Card", [&]() { opt = 1; }},
            {"Scan Mode", [&]() { opt = 2; }},
            {"Load File", [&]() { opt = 3; }},
            {"Brute Force", [&]() { opt = 4; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: {
                if (!initLfReader()) {
                    displayError("No LF reader found");
                    delay(1500);
                    break;
                }
                tft.fillScreen(bruceConfig.bgColor);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.drawCentreString("125kHz Read", tftWidth / 2, 5, 2);
                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawCentreString("Place card on reader...", tftWidth / 2, 25, 1);

                LfCardData card;
                while (!check(EscPress)) {
                    if (lfRfidRead(card)) {
                        lfRfidDisplayCard(card);

                        // Option to save
                        delay(500);
                        int action = 0;
                        options = {
                            {"Save", [&]() { action = 1; }},
                            {"Read Again", [&]() { action = 2; }},
                            {"Back", [&]() { action = 0; }},
                        };
                        loopOptions(options);

                        if (action == 1) {
                            String filename = keyboard(card.uid, 30, "Filename:");
                            if (lfRfidSave(card, filename)) {
                                displaySuccess("Saved!");
                            } else {
                                displayError("Save failed");
                            }
                            delay(1000);
                        }
                        if (action != 2) break;

                        tft.fillScreen(bruceConfig.bgColor);
                        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                        tft.drawCentreString("125kHz Read", tftWidth / 2, 5, 2);
                        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                        tft.drawCentreString("Place card...", tftWidth / 2, 25, 1);
                    }
                    delay(100);
                }
                deinitLfReader();
                break;
            }
            case 2: lfRfidScanMode(); break;
            case 3: {
                FS *fs;
                String filepath = loopSD(*fs, true, "lf125");
                if (filepath.length() > 0) {
                    LfCardData card;
                    if (lfRfidLoad(card, filepath)) {
                        tft.fillScreen(bruceConfig.bgColor);
                        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                        tft.drawCentreString("Loaded Card", tftWidth / 2, 5, 2);
                        lfRfidDisplayCard(card);
                        while (!check(EscPress) && !check(SelPress)) delay(100);
                    } else {
                        displayError("Load failed");
                        delay(1500);
                    }
                }
                break;
            }
            case 4: lfRfidBruteForce(); break;
            default: return;
        }
    }
}
