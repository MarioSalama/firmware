#pragma once
#include <Arduino.h>

#if (defined(MIC_SPM1423) || defined(MIC_INMP441)) && (defined(HAS_NS4168_SPKR) || defined(BUZZ_PIN))

// DTMF Decoder - listens via microphone, detects touch tones in real time
void dtmfDecoder();

// DTMF Encoder - generates touch tones via speaker
void dtmfEncoder();

// DTMF Dialer - dial a number sequence with tones
void dtmfDialer();

// Audio bug detector - scans for ultrasonic/RF interference via mic
void audioBugDetector();

// Ultrasonic jammer - emit high-freq tones to disrupt nearby microphones
void ultrasonicJammer();

// Main menu
void dtmfToolsMenu();

#else
inline void dtmfToolsMenu() {}
#endif
