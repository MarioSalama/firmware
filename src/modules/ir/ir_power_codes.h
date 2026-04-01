#pragma once
#include <stdint.h>

// Extended IR code database organized by device category
struct IrDeviceCode {
    const char *brand;
    const char *protocol; // "NEC", "Samsung", "RC5", "RC6", "Sony"
    uint16_t address;
    uint16_t command;
    const char *function;
};

// ── TV Power & Control codes ────────────────────────────────────────────────
static const IrDeviceCode IR_TV_CODES[] = {
    // Samsung
    {"Samsung", "NEC", 0x07, 0x02, "Power"},
    {"Samsung", "NEC", 0x07, 0x03, "Source"},
    {"Samsung", "NEC", 0x07, 0x07, "Vol+"},
    {"Samsung", "NEC", 0x07, 0x0B, "Vol-"},
    {"Samsung", "NEC", 0x07, 0x12, "Ch+"},
    {"Samsung", "NEC", 0x07, 0x10, "Ch-"},
    {"Samsung", "NEC", 0x07, 0x0D, "Mute"},
    {"Samsung", "NEC", 0x07, 0x98, "Power Alt"},
    // LG
    {"LG", "NEC", 0x04, 0x08, "Power"},
    {"LG", "NEC", 0x04, 0x02, "Vol+"},
    {"LG", "NEC", 0x04, 0x03, "Vol-"},
    {"LG", "NEC", 0x04, 0x00, "Ch+"},
    {"LG", "NEC", 0x04, 0x01, "Ch-"},
    {"LG", "NEC", 0x04, 0x09, "Mute"},
    {"LG", "NEC", 0x11, 0x08, "Power Alt"},
    // Sony
    {"Sony", "Sony", 0x01, 0x15, "Power"},
    {"Sony", "Sony", 0x01, 0x12, "Vol+"},
    {"Sony", "Sony", 0x01, 0x13, "Vol-"},
    {"Sony", "Sony", 0x01, 0x10, "Ch+"},
    {"Sony", "Sony", 0x01, 0x11, "Ch-"},
    {"Sony", "Sony", 0x01, 0x14, "Mute"},
    {"Sony", "Sony", 0x01, 0x95, "Power Alt"},
    // Philips
    {"Philips", "RC6", 0x00, 0x0C, "Power"},
    {"Philips", "RC6", 0x00, 0x10, "Vol+"},
    {"Philips", "RC6", 0x00, 0x11, "Vol-"},
    {"Philips", "RC6", 0x00, 0x20, "Ch+"},
    {"Philips", "RC6", 0x00, 0x21, "Ch-"},
    {"Philips", "RC6", 0x00, 0x0D, "Mute"},
    // Toshiba
    {"Toshiba", "NEC", 0x40, 0x12, "Power"},
    {"Toshiba", "NEC", 0x40, 0x1A, "Vol+"},
    {"Toshiba", "NEC", 0x40, 0x1E, "Vol-"},
    {"Toshiba", "NEC", 0x40, 0x1B, "Ch+"},
    {"Toshiba", "NEC", 0x40, 0x1F, "Ch-"},
    // Sharp
    {"Sharp", "NEC", 0x46, 0x11, "Power"},
    {"Sharp", "NEC", 0x46, 0x14, "Vol+"},
    {"Sharp", "NEC", 0x46, 0x15, "Vol-"},
    // Vizio
    {"Vizio", "NEC", 0x04, 0x08, "Power"},
    {"Vizio", "NEC", 0x04, 0x02, "Vol+"},
    {"Vizio", "NEC", 0x04, 0x03, "Vol-"},
    // Hisense
    {"Hisense", "NEC", 0x00, 0x08, "Power"},
    {"Hisense", "NEC", 0x00, 0x02, "Vol+"},
    {"Hisense", "NEC", 0x00, 0x03, "Vol-"},
    // TCL / Roku TV
    {"TCL/Roku", "NEC", 0x04, 0x08, "Power"},
    {"TCL/Roku", "NEC", 0x04, 0x02, "Vol+"},
    {"TCL/Roku", "NEC", 0x04, 0x03, "Vol-"},
    // Panasonic
    {"Panasonic", "NEC", 0x80, 0x3D, "Power"},
    {"Panasonic", "NEC", 0x80, 0x20, "Vol+"},
    {"Panasonic", "NEC", 0x80, 0x21, "Vol-"},
    // Sanyo
    {"Sanyo", "NEC", 0x1C, 0x08, "Power"},
    // JVC
    {"JVC", "NEC", 0x03, 0x40, "Power"},
    // Insignia
    {"Insignia", "NEC", 0x04, 0x08, "Power"},
    // Element
    {"Element", "NEC", 0x04, 0x08, "Power"},
    // Magnavox
    {"Magnavox", "RC6", 0x00, 0x0C, "Power"},
    // Emerson
    {"Emerson", "NEC", 0x1C, 0x08, "Power"},
    // Westinghouse
    {"Westinghouse", "NEC", 0x04, 0x08, "Power"},
    // Sceptre
    {"Sceptre", "NEC", 0x04, 0x08, "Power"},
    // Funai
    {"Funai", "NEC", 0x40, 0x12, "Power"},
    // Haier
    {"Haier", "NEC", 0x00, 0x08, "Power"},
    // RCA
    {"RCA", "NEC", 0x1C, 0x48, "Power"},
    // ONN
    {"ONN/Roku", "NEC", 0x04, 0x08, "Power"},
};
static constexpr int IR_TV_CODE_COUNT = sizeof(IR_TV_CODES) / sizeof(IR_TV_CODES[0]);

// ── AC / Climate codes ──────────────────────────────────────────────────────
static const IrDeviceCode IR_AC_CODES[] = {
    {"Generic AC", "NEC", 0xFF, 0x02, "Power"},
    {"Daikin", "NEC", 0x16, 0x0F, "Power"},
    {"Mitsubishi", "NEC", 0x23, 0xCB, "Power"},
    {"Fujitsu", "NEC", 0x14, 0x63, "Power"},
    {"Carrier", "NEC", 0x4E, 0x08, "Power"},
    {"Gree", "NEC", 0x00, 0x08, "Power"},
    {"Haier AC", "NEC", 0xA6, 0x02, "Power"},
    {"Midea", "NEC", 0xB2, 0x1F, "Power"},
    {"Trane", "NEC", 0x40, 0x12, "Power"},
    {"Whirlpool", "NEC", 0x12, 0x08, "Power"},
    {"LG AC", "NEC", 0x88, 0x0C, "Power"},
    {"Samsung AC", "NEC", 0x01, 0xD2, "Power"},
    {"Panasonic AC", "NEC", 0x10, 0x90, "Power"},
    {"Sharp AC", "NEC", 0xAA, 0x55, "Power"},
    {"Toshiba AC", "NEC", 0x43, 0x01, "Power"},
    {"Hitachi AC", "NEC", 0x28, 0x01, "Power"},
    {"York AC", "NEC", 0xF5, 0x08, "Power"},
    {"Electrolux", "NEC", 0x04, 0x08, "Power"},
};
static constexpr int IR_AC_CODE_COUNT = sizeof(IR_AC_CODES) / sizeof(IR_AC_CODES[0]);

// ── Projector codes ─────────────────────────────────────────────────────────
static const IrDeviceCode IR_PROJECTOR_CODES[] = {
    {"Epson", "NEC", 0x15, 0x11, "Power"},
    {"BenQ", "NEC", 0x30, 0x0A, "Power"},
    {"Optoma", "NEC", 0x34, 0x06, "Power"},
    {"ViewSonic", "NEC", 0x06, 0x0F, "Power"},
    {"Acer", "NEC", 0x30, 0x08, "Power"},
    {"InFocus", "NEC", 0x45, 0x02, "Power"},
    {"NEC Proj", "NEC", 0x24, 0x0D, "Power"},
    {"Sony Proj", "Sony", 0x54, 0x15, "Power"},
    {"Panasonic Proj", "NEC", 0x80, 0x3D, "Power"},
    {"Dell Proj", "NEC", 0x27, 0x10, "Power"},
    {"Casio Proj", "NEC", 0x03, 0x38, "Power"},
    {"LG Proj", "NEC", 0x04, 0x08, "Power"},
};
static constexpr int IR_PROJECTOR_CODE_COUNT = sizeof(IR_PROJECTOR_CODES) / sizeof(IR_PROJECTOR_CODES[0]);

// ── Soundbar / Audio codes ──────────────────────────────────────────────────
static const IrDeviceCode IR_AUDIO_CODES[] = {
    {"Bose", "NEC", 0x1C, 0x4C, "Power"},
    {"JBL", "NEC", 0x80, 0x0C, "Power"},
    {"Samsung Sound", "NEC", 0x0E, 0x0C, "Power"},
    {"Sony Sound", "Sony", 0x10, 0x15, "Power"},
    {"LG Sound", "NEC", 0x02, 0x08, "Power"},
    {"Yamaha", "NEC", 0x7A, 0x1E, "Power"},
    {"Denon", "NEC", 0x00, 0x0C, "Power"},
    {"Onkyo", "NEC", 0x4B, 0x0C, "Power"},
    {"Harman Kardon", "NEC", 0x80, 0x0C, "Power"},
    {"Sonos", "NEC", 0x04, 0x08, "Power"},
    {"Vizio Sound", "NEC", 0x01, 0x0C, "Power"},
    {"Polk", "NEC", 0x40, 0x0C, "Power"},
};
static constexpr int IR_AUDIO_CODE_COUNT = sizeof(IR_AUDIO_CODES) / sizeof(IR_AUDIO_CODES[0]);

// ── LED Strip Controller codes ──────────────────────────────────────────────
static const IrDeviceCode IR_LED_CODES[] = {
    {"LED Type 1", "NEC", 0x00, 0x45, "Power"},
    {"LED Type 2", "NEC", 0xEF, 0x00, "Power"},
    {"LED Type 3", "NEC", 0x00, 0x40, "Power"},
    {"LED Type 4", "NEC", 0xFF, 0x45, "Power"},
    {"LED Type 1", "NEC", 0x00, 0x46, "Bright+"},
    {"LED Type 1", "NEC", 0x00, 0x47, "Bright-"},
    {"LED Type 1", "NEC", 0x00, 0x44, "Red"},
    {"LED Type 1", "NEC", 0x00, 0x40, "Green"},
    {"LED Type 1", "NEC", 0x00, 0x43, "Blue"},
    {"LED Type 1", "NEC", 0x00, 0x07, "White"},
    {"LED Type 1", "NEC", 0x00, 0x04, "Flash"},
    {"LED Type 1", "NEC", 0x00, 0x05, "Strobe"},
    {"LED Type 1", "NEC", 0x00, 0x06, "Fade"},
    {"LED Type 1", "NEC", 0x00, 0x03, "Smooth"},
};
static constexpr int IR_LED_CODE_COUNT = sizeof(IR_LED_CODES) / sizeof(IR_LED_CODES[0]);

// ── Fan / Ceiling Fan codes ─────────────────────────────────────────────────
static const IrDeviceCode IR_FAN_CODES[] = {
    {"Generic Fan 1", "NEC", 0x00, 0x01, "Power"},
    {"Generic Fan 2", "NEC", 0x71, 0x01, "Power"},
    {"Dyson", "NEC", 0x01, 0x10, "Power"},
    {"Hunter", "NEC", 0x00, 0x0C, "Power"},
    {"Hampton Bay", "NEC", 0x42, 0x01, "Power"},
    {"Harbor Breeze", "NEC", 0x00, 0x02, "Power"},
    {"Minka Aire", "NEC", 0x04, 0x01, "Power"},
    {"Westinghouse Fan", "NEC", 0x71, 0x04, "Power"},
};
static constexpr int IR_FAN_CODE_COUNT = sizeof(IR_FAN_CODES) / sizeof(IR_FAN_CODES[0]);

// ── Set-top Box / Streaming codes ───────────────────────────────────────────
static const IrDeviceCode IR_STB_CODES[] = {
    {"Apple TV", "NEC", 0x87, 0x02, "Power"},
    {"Fire TV Stick", "NEC", 0x01, 0x40, "Power"},
    {"Chromecast", "NEC", 0x04, 0x08, "Power"},
    {"Xbox", "NEC", 0x80, 0x0C, "Power"},
    {"PS5", "NEC", 0x08, 0x15, "Power"},
    {"Cable Box 1", "NEC", 0x00, 0x0C, "Power"},
    {"Cable Box 2", "RC5", 0x00, 0x0C, "Power"},
    {"DirecTV", "RC6", 0x17, 0x0C, "Power"},
    {"Dish Network", "NEC", 0x00, 0x08, "Power"},
};
static constexpr int IR_STB_CODE_COUNT = sizeof(IR_STB_CODES) / sizeof(IR_STB_CODES[0]);
