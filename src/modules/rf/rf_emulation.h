#pragma once
#include <Arduino.h>

// RF Signal Emulation & Replay System
// Provides enhanced replay, protocol emulation, and signal manipulation

// ── Signal Replay with Modification ─────────────────────────────────────────

// Replay a captured signal with optional modifications
// freq: frequency in MHz, key: captured code, protocol: RCSwitch protocol number
// te: pulse length, repeat: number of repeats
void rfReplaySignal(float freq, uint64_t key, int protocol, int te, int repeat = 5);

// Replay with rolling code step (for KeeLoq signals)
void rfReplayWithStep(float freq, uint64_t key, int protocol, int te, int step);

// ── Protocol Emulators ──────────────────────────────────────────────────────

// Garage door protocol emulator menu
void rfGarageDoorEmulator();

// Gate/barrier remote emulator
void rfGateEmulator();

// Wireless doorbell emulator
void rfDoorbellEmulator();

// Car key fob emulator (fixed code only - for educational/authorized testing)
void rfKeyFobEmulator();

// ── Enhanced Brute Force ────────────────────────────────────────────────────

// Extended brute force with more protocols than the base 6
void rfEnhancedBruteForce();

// ── Signal Analysis ─────────────────────────────────────────────────────────

// Capture and analyze a signal, showing decoded protocol info
void rfSignalAnalyzer();

// ── Main Menu ───────────────────────────────────────────────────────────────
void rfEmulationMenu();
