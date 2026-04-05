#pragma once
#if !defined(LITE_VERSION)
#include <Arduino.h>

// WiFi Probe Request Tracker - passively tracks nearby devices
// by monitoring WiFi probe requests they broadcast

// Main probe tracker UI
void wifiProbeTracker();

// Signal intelligence dashboard - shows all radio activity
void signalIntelDashboard();

// WiFi client tracker - track connected devices on a network
void wifiClientTracker();

// Probe tracker menu
void wifiProbeMenu();

#endif
