#pragma once
#include <Arduino.h>

enum IrBruteMode {
    IR_BRUTE_ALL_POWER,      // Try all known power codes across all brands
    IR_BRUTE_NEC_SCAN,       // Scan NEC address:command space
    IR_BRUTE_SAMSUNG_SCAN,   // Scan Samsung space
    IR_BRUTE_RC5_SCAN,       // Scan RC5 space
    IR_BRUTE_RC6_SCAN,       // Scan RC6 space
    IR_BRUTE_SONY_SCAN,      // Scan Sony SIRC space
    IR_BRUTE_CUSTOM,         // Custom protocol + address range
    IR_BRUTE_BY_CATEGORY,    // Brute by device category (TV, AC, etc.)
};

// Launch IR brute force interactive menu
void irBruteForce();

// Programmatic brute force (for scripting/serial commands)
void irBruteForceStart(IrBruteMode mode, uint16_t startAddr = 0, uint16_t endAddr = 0xFF,
                       uint16_t startCmd = 0, uint16_t endCmd = 0xFF, int delayMs = 100);
