#pragma once
/**
 * @file tembed_status_bar.h
 * @brief Enhanced status bar and multi-radio dashboard for T-Embed CC1101
 *
 * Provides board-specific UI enhancements:
 * - Enhanced status bar with CC1101 frequency, active radio indicators
 * - Multi-radio dashboard showing all wireless interfaces
 * - Quick settings overlay accessible from any screen
 */

#include <Arduino.h>

#ifdef T_EMBED_1101

// ── Enhanced Status Bar ─────────────────────────────────────────────────────

// Draw the enhanced status bar with T-Embed specific info
// Call this instead of or in addition to the standard drawStatusBar()
void drawEnhancedStatusBar();

// ── Multi-Radio Dashboard ───────────────────────────────────────────────────

// Show full-screen dashboard of all radio interfaces
// WiFi, BLE, CC1101, NRF24, IR, PN532 status at a glance
void showRadioDashboard();

// ── Quick Settings Overlay ──────────────────────────────────────────────────

// Show compact settings overlay (brightness, volume, frequency)
void showQuickSettings();

// ── Frequency Display ───────────────────────────────────────────────────────

// Draw CC1101 frequency indicator in status bar area
void drawFreqIndicator(float freqMHz);

// ── Signal Strength Bars ────────────────────────────────────────────────────

// Draw WiFi signal strength bars (like a phone)
void drawSignalBars(int x, int y, int strength, uint16_t color);

#else
inline void drawEnhancedStatusBar() {}
inline void showRadioDashboard() {}
inline void showQuickSettings() {}
#endif
