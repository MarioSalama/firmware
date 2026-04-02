#ifndef __RF_REPLAY_H__
#define __RF_REPLAY_H__

#include "rf_utils.h"
#include "structs.h"

// Main entry point menu
void rfReplayMenu();

// Signal capture & replay
void rfSignalCapture();
void rfSignalReplay();
void rfMultiCapture();

// Jam & replay attack
void rfJamAndReplay();

// De Bruijn sequence brute force
void rfDeBruijnAttack();

// Gate/door specific attacks
void rfGateAttack();

// Frequency scanner
void rfFrequencyScanner();

#endif
