#ifndef __BADUSB_ADVANCED_H__
#define __BADUSB_ADVANCED_H__

#if !defined(LITE_VERSION) && defined(USB_as_HID)

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <globals.h>
#include <USBHIDKeyboard.h>
#include "modules/badusb_ble/ducky_typer.h"

// ── DuckyScript 2.0 Variable ────────────────────────────────────────────────
struct DuckyVar {
    String name;
    int value;
};

// ── DuckyScript 2.0 Function ────────────────────────────────────────────────
struct DuckyFunction {
    String name;
    int startLine;
    int endLine;
};

// ── Payload descriptor ──────────────────────────────────────────────────────
struct PayloadEntry {
    const char *name;
    const char *description;
    void (*generate)(String &out, const String &param1, const String &param2);
};

// ── Target OS enum ──────────────────────────────────────────────────────────
enum TargetOS : uint8_t {
    OS_WINDOWS = 0,
    OS_LINUX,
    OS_MACOS,
    OS_COUNT
};

// ── Enhanced DuckyScript 2.0 Engine ─────────────────────────────────────────
class DuckyEngine {
public:
    DuckyEngine(HIDInterface *hid);
    ~DuckyEngine();

    // Execute a full script from a string buffer
    bool executeScript(const String &script);

    // Execute a script from a file on the given filesystem
    bool executeFile(FS &fs, const String &path);

    // Execute a single raw DuckyScript line (delegates to existing parser for
    // basic commands, handles 2.0 extensions internally)
    bool executeLine(const String &line);

    // Stop execution
    void stop();
    bool isRunning() const { return _running; }

private:
    HIDInterface *_hid;
    bool _running;

    // Script lines for random access (needed by IF/WHILE/FUNCTION)
    std::vector<String> _lines;
    int _pc; // program counter (current line index)

    // DuckyScript 2.0 state
    std::vector<DuckyVar> _vars;
    std::vector<DuckyFunction> _funcs;
    int _defaultDelay;

    // Variable helpers
    int  getVar(const String &name);
    void setVar(const String &name, int value);
    bool evalCondition(const String &expr);
    int  evalExpression(const String &expr);

    // Pre-scan for FUNCTIONs so they can be called
    void scanFunctions();

    // Execute from _pc until end / RETURN / break
    bool run();
};

// ── Menu entry points ───────────────────────────────────────────────────────

// Main advanced BadUSB menu
void badusbAdvancedMenu();

// Sub-menus
void badusbRunScriptMenu();
void badusbPayloadsMenu();
void badusbQuickAttack();
void badusbPayloadBuilder();
void badusbDeviceSpoofMenu();

// Payload sub-menus per OS
void badusbWindowsPayloads();
void badusbLinuxPayloads();
void badusbMacPayloads();

// ── Payload generators (populate a String with DuckyScript) ─────────────────
// Windows
void payloadWifiExfil(String &out);
void payloadWinReverseShell(String &out, const String &ip, const String &port);
void payloadDisableDefender(String &out);
void payloadAddAdminUser(String &out, const String &user, const String &pass);
void payloadCredHarvester(String &out);
void payloadDownloadExec(String &out, const String &url);

// Linux
void payloadLinuxReverseShell(String &out, const String &ip, const String &port);
void payloadAddSudoer(String &out, const String &user, const String &pass);
void payloadSSHKeyInject(String &out, const String &pubkey);
void payloadCronPersistence(String &out, const String &ip, const String &port);

// macOS
void payloadMacReverseShell(String &out, const String &ip, const String &port);
void payloadLaunchAgentPersist(String &out, const String &ip, const String &port);
void payloadKeychainDump(String &out);

// Quick attacks
void quickOpenTerminal(TargetOS os);
void quickRickRoll();
void quickForkBomb(TargetOS os);
void quickWallChange(TargetOS os);
void quickCustomCommand();

// ── Helpers ─────────────────────────────────────────────────────────────────
bool savePayloadToSD(const String &script, const String &filename);
void executePayloadString(const String &script);
String promptInput(const String &title, const String &defaultVal = "");

#endif // !LITE_VERSION && USB_as_HID
#endif // __BADUSB_ADVANCED_H__
