#pragma once
#include <Arduino.h>

#ifdef USE_BQ27220_VIA_I2C

struct BatteryInfo {
    int chargePercent;      // 0-100%
    int voltage;            // mV
    int current;            // mA (positive=charging, negative=discharging)
    int temperature;        // 0.1K units (raw from BQ27220)
    int remainingCapacity;  // mAh
    int fullChargeCapacity; // mAh
    int designCapacity;     // mAh
    int cycleCount;         // charge cycles
    bool isCharging;
    int timeToEmpty;        // minutes (0 if charging)
    int timeToFull;         // minutes (0 if discharging)
    int healthPercent;      // battery health (fullCharge/design * 100)
};

// Get comprehensive battery info
BatteryInfo getBatteryInfo();

// Show the battery dashboard screen (blocking, returns on back press)
void showBatteryDashboard();

// Estimate runtime in minutes for a given power mode
int estimateRuntime(const char* mode);

#else
inline void showBatteryDashboard() {}
#endif
