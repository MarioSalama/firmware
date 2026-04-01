#pragma once
/**
 * @file lf_rfid.h
 * @brief Low-Frequency (125kHz) RFID support via PN5180 for T-Embed CC1101
 *
 * The PN5180 is a full NFC frontend that also supports ISO 15693 and can be
 * configured for 125kHz operation with external antenna tuning. When connected
 * via SPI (using the QWIIC/expansion port), it enables reading and emulating
 * common LF proximity cards:
 *   - EM4100 / EM4102 (most common 125kHz cards/fobs)
 *   - HID Prox (26-bit H10301 format)
 *   - AWID
 *   - Indala
 *
 * This module provides read, save, load, and replay functionality for old
 * 125kHz fobs and cards commonly used in access control systems.
 */

#include <Arduino.h>

// ── Card Data Structures ────────────────────────────────────────────────────

enum LfCardType {
    LF_UNKNOWN = 0,
    LF_EM4100,      // EM4100/4102 - most common 125kHz
    LF_HID_PROX,    // HID ProxCard II / ISOProx
    LF_AWID,        // AWID proximity
    LF_INDALA,      // Indala FlexSecur
};

struct LfCardData {
    LfCardType type;
    uint64_t rawData;       // Raw demodulated bits
    uint32_t facilityCode;  // Facility/site code
    uint32_t cardNumber;    // Card number
    uint8_t bitLength;      // Number of valid bits
    String uid;             // Printable UID string
};

// ── Main Functions ──────────────────────────────────────────────────────────

// Launch the LF RFID interactive menu
void lfRfidMenu();

// Read a single LF card (blocking until card detected or escape)
bool lfRfidRead(LfCardData &card);

// Save card data to file
bool lfRfidSave(const LfCardData &card, const String &filename);

// Load card data from file
bool lfRfidLoad(LfCardData &card, const String &filename);

// Continuous scan mode - display cards as they're detected
void lfRfidScanMode();

// Brute force mode - try common facility codes
void lfRfidBruteForce();

// Display card info on screen
void lfRfidDisplayCard(const LfCardData &card);
