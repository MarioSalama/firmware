#pragma once

#ifdef T_EMBED_1101

#include <Arduino.h>
#include <vector>

struct BootProfile {
    const char* name;
    const char* description;
    void (*activate)();
};

// Check if user is holding encoder during boot, show profile picker if so
// Returns true if a profile was selected and launched
bool checkBootProfileSelection();

// Get list of available profiles
const std::vector<BootProfile>& getBootProfiles();

#endif // T_EMBED_1101
