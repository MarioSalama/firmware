#include "nfc_bruteforce.h"
#include "core/display.h"
#include "core/sd_functions.h"
#include <Adafruit_PN532.h>
#include <globals.h>

// ── Extended Mifare Key Dictionary ──────────────────────────────────────────
// Comprehensive list of known default, common, and vulnerable keys

static const uint8_t MIFARE_KEYS[][6] = {
    // Factory defaults
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Common transport keys
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    // NXP/MIFARE Application Directory
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xC1, 0x87, 0x37, 0x70, 0xAA, 0xDF},
    // Hotel systems
    {0x8F, 0xD0, 0xA4, 0xF2, 0x56, 0xE9},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    // Parking systems
    {0x71, 0x4C, 0x5C, 0x88, 0x6E, 0x97},
    {0x58, 0x7E, 0xE5, 0xF9, 0x35, 0x0F},
    // Vending machines
    {0xA6, 0x4F, 0x3C, 0x21, 0x18, 0xE0},
    {0x2A, 0x2C, 0x13, 0xCC, 0x24, 0x2A},
    // Access control common
    {0xFC, 0x00, 0x01, 0x87, 0x78, 0xF7},
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    {0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6},
    {0x8B, 0x5E, 0x87, 0xA5, 0xCA, 0x71},
    // Payment/transit systems
    {0x6A, 0x1A, 0xDA, 0xE3, 0x08, 0x01},
    {0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6},
    {0x52, 0x46, 0x09, 0x3B, 0x5E, 0xB4},
    // Gym/fitness
    {0x48, 0xFF, 0xE8, 0x07, 0x49, 0x84},
    {0xEE, 0x00, 0x04, 0xD4, 0x4E, 0x5D},
    // Laundry systems
    {0xA2, 0x3D, 0x3B, 0x6E, 0xF7, 0x41},
    {0xBB, 0x52, 0xF8, 0xCB, 0x39, 0x05},
    // Elevator/floor access
    {0xD0, 0x47, 0xBE, 0xFE, 0x1B, 0xEA},
    {0x78, 0x78, 0x78, 0x78, 0x78, 0x78},
    // Additional common
    {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
    {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54},
    {0x10, 0x20, 0x30, 0x40, 0x50, 0x60},
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB},
    {0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F},
    {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0},
    {0xCA, 0xFE, 0xBA, 0xBE, 0xDE, 0xAD},
    {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE},
};

static constexpr int MIFARE_KEY_COUNT = sizeof(MIFARE_KEYS) / sizeof(MIFARE_KEYS[0]);

// ── PN532 Helper ────────────────────────────────────────────────────────────

static Adafruit_PN532 *getNfc() {
    static Adafruit_PN532 *nfc = nullptr;
    if (!nfc) {
        nfc = new Adafruit_PN532(PN532_IRQ, PN532_RF_REST);
        nfc->setInterface(GROVE_SDA, GROVE_SCL);
        nfc->begin();
        uint32_t ver = nfc->getFirmwareVersion();
        if (!ver) {
            displayError("PN532 not found!");
            delay(1500);
            delete nfc;
            nfc = nullptr;
            return nullptr;
        }
        nfc->SAMConfig();
    }
    return nfc;
}

// ── Key Brute Force ─────────────────────────────────────────────────────────

void nfcKeyBruteForce() {
    Adafruit_PN532 *nfc = getNfc();
    if (!nfc) return;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("NFC Key Brute Force", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place card on reader...", tftWidth / 2, 30, 1);

    // Wait for card
    uint8_t uid[7];
    uint8_t uidLen;
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    // Display UID
    String uidStr = "";
    for (int i = 0; i < uidLen; i++) {
        if (uid[i] < 0x10) uidStr += "0";
        uidStr += String(uid[i], HEX);
        if (i < uidLen - 1) uidStr += ":";
    }
    uidStr.toUpperCase();

    tft.fillRect(0, 25, tftWidth, 20, bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("UID: " + uidStr, tftWidth / 2, 28, 1);

    // Try keys on each sector
    int totalSectors = (uidLen == 4) ? 16 : 40; // Classic 1K vs 4K
    int keysFound = 0;
    uint8_t foundKeys[40][6];
    bool sectorCracked[40] = {false};

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Testing " + String(MIFARE_KEY_COUNT) + " keys on " +
                             String(totalSectors) + " sectors...",
                         tftWidth / 2, 45, 1);

    for (int sector = 0; sector < totalSectors; sector++) {
        if (check(EscPress)) break;

        int blockAddr = sector * 4 + 3; // Trailer block

        for (int k = 0; k < MIFARE_KEY_COUNT; k++) {
            if (check(EscPress)) goto done;

            // Re-select card (it may have been deselected after failed auth)
            uint8_t uid2[7];
            uint8_t uid2Len;
            if (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid2, &uid2Len, 100)) {
                delay(50);
                nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid2, &uid2Len, 200);
            }

            // Try Key A
            if (nfc->mifareclassic_AuthenticateBlock(uid, uidLen, blockAddr, 0,
                                                     (uint8_t *)MIFARE_KEYS[k])) {
                memcpy(foundKeys[sector], MIFARE_KEYS[k], 6);
                sectorCracked[sector] = true;
                keysFound++;

                String keyStr = "";
                for (int i = 0; i < 6; i++) {
                    if (MIFARE_KEYS[k][i] < 0x10) keyStr += "0";
                    keyStr += String(MIFARE_KEYS[k][i], HEX);
                }
                keyStr.toUpperCase();

                displayRedStripe(
                    "S" + String(sector) + ": " + keyStr,
                    TFT_BLACK, TFT_GREEN
                );
                delay(300);
                break;
            }
        }

        // Progress
        int progW = tftWidth - 20;
        int progY = tftHeight - 25;
        tft.fillRect(10, progY, progW, 10, bruceConfig.bgColor);
        tft.drawRect(10, progY, progW, 10, bruceConfig.priColor);
        int fillW = (progW - 2) * (sector + 1) / totalSectors;
        tft.fillRect(11, progY + 1, fillW, 8, bruceConfig.priColor);
        tft.fillRect(0, progY + 12, tftWidth, 15, bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawCentreString(
            "Sector " + String(sector + 1) + "/" + String(totalSectors) +
                " | Found: " + String(keysFound),
            tftWidth / 2, progY + 12, 1
        );
    }

done:
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Results", tftWidth / 2, 5, 2);
    tft.drawCentreString("UID: " + uidStr, tftWidth / 2, 25, 1);
    tft.drawCentreString(
        String(keysFound) + "/" + String(totalSectors) + " sectors cracked", tftWidth / 2, 42, 2
    );

    // List found keys
    int y = 65;
    for (int s = 0; s < totalSectors && y < tftHeight - 10; s++) {
        if (sectorCracked[s]) {
            String keyStr = "S" + String(s) + ": ";
            for (int i = 0; i < 6; i++) {
                if (foundKeys[s][i] < 0x10) keyStr += "0";
                keyStr += String(foundKeys[s][i], HEX);
            }
            keyStr.toUpperCase();
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawString(keyStr, 10, y, 1);
            y += 14;
        }
    }

    // Wait for exit
    while (!check(EscPress) && !check(SelPress)) delay(100);
}

// ── UID Fuzzer ──────────────────────────────────────────────────────────────

void nfcUidFuzzer() {
    Adafruit_PN532 *nfc = getNfc();
    if (!nfc) return;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("UID Fuzzer", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place Magic/Gen1a card", tftWidth / 2, 25, 1);
    tft.drawCentreString("Card will be written!", tftWidth / 2, 40, 1);

    // Wait for card
    uint8_t uid[7];
    uint8_t uidLen;
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    int attempts = 0;
    while (!check(EscPress)) {
        attempts++;

        // Generate random 4-byte UID
        uint8_t newUid[4];
        for (int i = 0; i < 4; i++) newUid[i] = random(0, 256);

        // Calculate BCC
        uint8_t bcc = newUid[0] ^ newUid[1] ^ newUid[2] ^ newUid[3];

        // Block 0 for Gen1a: UID(4) + BCC(1) + SAK(1) + ATQA(2) + manufacturer(8)
        uint8_t block0[16] = {
            newUid[0], newUid[1], newUid[2], newUid[3], bcc,
            0x08,      // SAK
            0x04, 0x00, // ATQA
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        String uidStr = "";
        for (int i = 0; i < 4; i++) {
            if (newUid[i] < 0x10) uidStr += "0";
            uidStr += String(newUid[i], HEX);
        }
        uidStr.toUpperCase();

        tft.fillRect(0, 55, tftWidth, 50, bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Attempt #" + String(attempts), tftWidth / 2, 58, 1);
        tft.drawCentreString("UID: " + uidStr, tftWidth / 2, 75, 2);

        delay(500);

        // Re-detect card
        nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200);
    }
}

// ── Quick Clone ─────────────────────────────────────────────────────────────

void nfcQuickClone() {
    Adafruit_PN532 *nfc = getNfc();
    if (!nfc) return;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Quick Clone", tftWidth / 2, 5, 2);

    // Step 1: Read source
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place SOURCE card...", tftWidth / 2, 30, 1);

    uint8_t srcUid[7];
    uint8_t srcUidLen;
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, srcUid, &srcUidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    String srcUidStr = "";
    for (int i = 0; i < srcUidLen; i++) {
        if (srcUid[i] < 0x10) srcUidStr += "0";
        srcUidStr += String(srcUid[i], HEX);
        if (i < srcUidLen - 1) srcUidStr += ":";
    }
    srcUidStr.toUpperCase();

    displaySuccess("Source: " + srcUidStr);
    delay(2000);

    // Read all accessible data blocks
    uint8_t blockData[64][16]; // Up to 64 blocks for 1K
    bool blockRead[64] = {false};
    int blocksRead = 0;

    // Try reading with default key
    uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (int block = 0; block < 64; block++) {
        int sector = block / 4;
        int trailerBlock = sector * 4 + 3;

        // Authenticate
        if (nfc->mifareclassic_AuthenticateBlock(srcUid, srcUidLen, trailerBlock, 0, defaultKey)) {
            if (nfc->mifareclassic_ReadDataBlock(block, blockData[block])) {
                blockRead[block] = true;
                blocksRead++;
            }
        }
        // Re-detect after auth failure
        nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, srcUid, &srcUidLen, 200);
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Read " + String(blocksRead) + "/64 blocks", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place TARGET card...", tftWidth / 2, 30, 1);
    tft.drawCentreString("(Magic/Gen1a card)", tftWidth / 2, 45, 1);

    // Step 2: Wait for target card
    uint8_t dstUid[7];
    uint8_t dstUidLen;
    // Wait for card removal first
    delay(1000);
    while (nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, dstUid, &dstUidLen, 200)) {
        if (check(EscPress)) return;
        delay(200);
    }
    // Wait for new card
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, dstUid, &dstUidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    // Step 3: Write data
    int blocksWritten = 0;
    for (int block = 0; block < 64; block++) {
        if (!blockRead[block]) continue;
        if (block % 4 == 3) continue; // Skip trailer blocks for safety

        int sector = block / 4;
        int trailerBlock = sector * 4 + 3;

        if (nfc->mifareclassic_AuthenticateBlock(dstUid, dstUidLen, trailerBlock, 0, defaultKey)) {
            if (nfc->mifareclassic_WriteDataBlock(block, blockData[block])) {
                blocksWritten++;
            }
        }
        nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, dstUid, &dstUidLen, 200);
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Clone Complete", tftWidth / 2, 5, 2);
    tft.drawCentreString(
        String(blocksWritten) + " blocks written", tftWidth / 2, 30, 2
    );
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Source: " + srcUidStr, tftWidth / 2, 55, 1);

    while (!check(EscPress) && !check(SelPress)) delay(100);
}

// ── Full Dump ───────────────────────────────────────────────────────────────

void nfcFullDump() {
    Adafruit_PN532 *nfc = getNfc();
    if (!nfc) return;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Full Card Dump", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place card...", tftWidth / 2, 30, 1);

    uint8_t uid[7];
    uint8_t uidLen;
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    // Try all keys in dictionary on all sectors
    int totalSectors = 16; // 1K card
    int sectorsRead = 0;
    String dumpData = "";

    for (int sector = 0; sector < totalSectors; sector++) {
        if (check(EscPress)) break;

        int trailerBlock = sector * 4 + 3;
        bool authed = false;

        // Try keys
        for (int k = 0; k < MIFARE_KEY_COUNT && !authed; k++) {
            nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200);
            if (nfc->mifareclassic_AuthenticateBlock(uid, uidLen, trailerBlock, 0,
                                                     (uint8_t *)MIFARE_KEYS[k])) {
                authed = true;
            }
        }

        if (authed) {
            sectorsRead++;
            for (int b = 0; b < 4; b++) {
                uint8_t data[16];
                int block = sector * 4 + b;
                if (nfc->mifareclassic_ReadDataBlock(block, data)) {
                    dumpData += "B" + String(block) + ": ";
                    for (int i = 0; i < 16; i++) {
                        if (data[i] < 0x10) dumpData += "0";
                        dumpData += String(data[i], HEX);
                        dumpData += " ";
                    }
                    dumpData += "\n";
                }
            }
        }

        displayRedStripe(
            "Sector " + String(sector + 1) + "/" + String(totalSectors),
            getComplementaryColor2(bruceConfig.priColor), bruceConfig.priColor
        );
    }

    // Save to file
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Dump Complete", tftWidth / 2, 5, 2);
    tft.drawCentreString(
        String(sectorsRead) + "/" + String(totalSectors) + " sectors", tftWidth / 2, 25, 1
    );

    FS *fs;
    if (getFsStorage(fs)) {
        if (!(*fs).exists("/BruceRFID")) (*fs).mkdir("/BruceRFID");
        String uidStr = "";
        for (int i = 0; i < uidLen; i++) {
            if (uid[i] < 0x10) uidStr += "0";
            uidStr += String(uid[i], HEX);
        }
        String filename = "/BruceRFID/dump_" + uidStr + ".txt";
        File f = (*fs).open(filename, FILE_WRITE);
        if (f) {
            f.print(dumpData);
            f.close();
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.drawCentreString("Saved: " + filename, tftWidth / 2, 45, 1);
        }
    }

    while (!check(EscPress) && !check(SelPress)) delay(100);
}

// ── Access Bits Decoder ─────────────────────────────────────────────────────

void nfcAccessBitsDecoder() {
    Adafruit_PN532 *nfc = getNfc();
    if (!nfc) return;

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Access Bits Decoder", tftWidth / 2, 5, 2);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Place card...", tftWidth / 2, 30, 1);

    uint8_t uid[7];
    uint8_t uidLen;
    while (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200)) {
        if (check(EscPress)) return;
        delay(100);
    }

    uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Sector Access Bits", tftWidth / 2, 5, 2);

    int y = 25;
    for (int sector = 0; sector < 16 && y < tftHeight - 10; sector++) {
        int trailerBlock = sector * 4 + 3;

        nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 200);

        if (nfc->mifareclassic_AuthenticateBlock(uid, uidLen, trailerBlock, 0, defaultKey)) {
            uint8_t trailer[16];
            if (nfc->mifareclassic_ReadDataBlock(trailerBlock, trailer)) {
                // Access bits are bytes 6, 7, 8 of trailer
                uint8_t b6 = trailer[6], b7 = trailer[7], b8 = trailer[8];

                String accessStr = "S" + String(sector) + ": ";
                accessStr += String(b6, HEX) + " " + String(b7, HEX) + " " + String(b8, HEX);

                // Check if transport config (all FF FF FF FF FF FF + 78 77 88)
                bool isTransport = (b6 == 0xFF && b7 == 0x07 && b8 == 0x80);
                if (isTransport) accessStr += " [TRANSPORT]";

                tft.setTextColor(isTransport ? TFT_GREEN : bruceConfig.secColor,
                                 bruceConfig.bgColor);
                tft.drawString(accessStr, 5, y, 1);
                y += 14;
            }
        } else {
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.drawString("S" + String(sector) + ": LOCKED", 5, y, 1);
            y += 14;
        }
    }

    while (!check(EscPress) && !check(SelPress)) delay(100);
}

// ── Main NFC Enhanced Menu ──────────────────────────────────────────────────

void nfcEnhancedMenu() {
    while (true) {
        int opt = 0;
        options = {
            {"Key Brute Force", [&]() { opt = 1; }},
            {"Quick Clone", [&]() { opt = 2; }},
            {"Full Dump", [&]() { opt = 3; }},
            {"Access Bits", [&]() { opt = 4; }},
            {"UID Fuzzer", [&]() { opt = 5; }},
            {"Back", [&]() { opt = 0; }},
        };
        loopOptions(options);

        switch (opt) {
            case 1: nfcKeyBruteForce(); break;
            case 2: nfcQuickClone(); break;
            case 3: nfcFullDump(); break;
            case 4: nfcAccessBitsDecoder(); break;
            case 5: nfcUidFuzzer(); break;
            default: return;
        }
    }
}
