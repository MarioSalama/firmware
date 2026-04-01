#pragma once
#include <Arduino.h>

// NFC/RFID Brute Force and Enhanced Tools
// For Mifare Classic key cracking, UID manipulation, and card emulation

// ── Key Brute Force ─────────────────────────────────────────────────────────

// Brute force Mifare Classic keys with extended key dictionary
void nfcKeyBruteForce();

// ── UID Tools ───────────────────────────────────────────────────────────────

// Generate and try random UIDs for access testing
void nfcUidFuzzer();

// UID cloning - read then write to magic card
void nfcQuickClone();

// ── Card Analysis ───────────────────────────────────────────────────────────

// Full card dump with key recovery
void nfcFullDump();

// Access bits decoder - show human-readable access permissions
void nfcAccessBitsDecoder();

// ── Main Menu ───────────────────────────────────────────────────────────────
void nfcEnhancedMenu();
