// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "preset.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace preset {
static void fail(const std::string& text) { throw std::runtime_error(text); }
std::string trim(const std::string& t) {
    auto a = t.find_first_not_of(" \t");
    return a == std::string::npos ? "" : t.substr(a, t.find_last_not_of(" \t") - a + 1);
}
bool valid_utf8(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        unsigned c = static_cast<unsigned char>(text[i++]), n = 0, v = 0, minimum = 0;
        if (c < 128) continue;
        if (c >= 0xc2 && c <= 0xdf) { n = 1; v = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { n = 2; v = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { n = 3; v = c & 7; minimum = 0x10000; }
        else return false;
        if (i + n > text.size()) return false;
        while (n--) { unsigned b = static_cast<unsigned char>(text[i++]); if ((b & 192) != 128) return false; v = (v << 6) | (b & 63); }
        if (v < minimum || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)) return false;
    }
    return true;
}
static void utf8(std::string& out, unsigned v) {
    if (v < 128) out += static_cast<char>(v);
    else if (v < 0x800) { out += static_cast<char>(0xc0 | (v >> 6)); out += static_cast<char>(0x80 | (v & 63)); }
    else if (v < 0x10000) { out += static_cast<char>(0xe0 | (v >> 12)); out += static_cast<char>(0x80 | ((v >> 6) & 63)); out += static_cast<char>(0x80 | (v & 63)); }
    else { out += static_cast<char>(0xf0 | (v >> 18)); out += static_cast<char>(0x80 | ((v >> 12) & 63)); out += static_cast<char>(0x80 | ((v >> 6) & 63)); out += static_cast<char>(0x80 | (v & 63)); }
}
Value parse_value(const std::string& raw) {
    const auto s = trim(raw);
    if (s == "true") return true;
    if (s == "false") return false;
    static const std::regex numberPattern(R"(-?(0|[1-9][0-9]*)(\.[0-9]+)?|0x[0-9a-fA-F]+)");
    if (std::regex_match(s, numberPattern)) {
        try {
            if (s.rfind("0x", 0) == 0) {
                long double n = 0;
                for (size_t i = 2; i < s.size(); ++i) n = n * 16 + (s[i] <= '9' ? s[i] - '0' : (s[i] | 32) - 'a' + 10);
                if (!std::isfinite(n) || n > 4294967295.0L) fail("number overflow");
                return static_cast<double>(n);
            }
            std::istringstream in(s); in.imbue(std::locale::classic()); double n; in >> n;
            if (!in || !std::isfinite(n)) fail("number overflow");
            return n;
        } catch (...) { fail("invalid numeric value"); }
    }
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') fail("expected quoted string, number or bool");
    std::string out;
    auto hex = [&](size_t& i) {
        unsigned v = 0;
        for (unsigned j = 0; j < 4; ++j) {
            if (++i >= s.size() - 1) fail("incomplete Unicode escape");
            char c = s[i]; unsigned d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if ((c | 32) >= 'a' && (c | 32) <= 'f') d = (c | 32) - 'a' + 10;
            else { fail("invalid Unicode escape"); d = 0; }
            v = v * 16 + d;
        } return v;
    };
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 32 || c == '"') fail("invalid string character");
        if (c != '\\') { out += static_cast<char>(c); continue; }
        if (++i >= s.size() - 1) fail("incomplete escape");
        switch (s[i]) {
        case '"': case '\\': case '/': out += s[i]; break;
        case 'b': out += '\b'; break; case 'f': out += '\f'; break;
        case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
        case 'u': {
            unsigned v = hex(i);
            if (v >= 0xd800 && v <= 0xdbff) {
                if (i + 2 >= s.size() - 1 || s[++i] != '\\' || s[++i] != 'u') fail("missing low surrogate");
                unsigned low = hex(i); if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
                v = 0x10000 + ((v - 0xd800) << 10) + low - 0xdc00;
            } else if (v >= 0xdc00 && v <= 0xdfff) fail("unpaired surrogate");
            utf8(out, v); break;
        }
        default: fail("unknown string escape");
        }
    }
    if (!valid_utf8(out)) fail("invalid UTF-8");
    return out;
}
std::string format_value(const Value& v) {
    if (const auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
    if (const auto* n = std::get_if<double>(&v)) {
        if (!std::isfinite(*n)) fail("non-finite number");
        int precision = *n == 0 ? 0 : std::max(0, 16 - static_cast<int>(std::floor(std::log10(std::abs(*n)))));
        std::ostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(precision) << *n;
        auto s = out.str();
        if (s.find('.') != std::string::npos) { while (s.back() == '0') s.pop_back(); if (s.back() == '.') s.pop_back(); }
        return s == "-0" ? "0" : s;
    }
    std::string out = "\"";
    for (unsigned char c : std::get<std::string>(v)) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { const char* h = "0123456789abcdef"; out += "\\u00"; out += h[c >> 4]; out += h[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
const std::vector<std::string>& inputs() {
    static const std::vector<std::string> v = {"UP","DOWN","RIGHT","LEFT","CROSS","CIRCLE","R2","L2","SQUARE","TRIANGLE","R1","L1","SELECT","START","L3","R3"}; return v;
}
const std::vector<std::string>& actions() {
    static const std::vector<std::string> v = {
        "NONE", "BUTTON_PRESS_UP", "BUTTON_PRESS_DOWN", "BUTTON_PRESS_LEFT", "BUTTON_PRESS_RIGHT",
        "BUTTON_PRESS_B1", "BUTTON_PRESS_B2", "BUTTON_PRESS_B3", "BUTTON_PRESS_B4", "BUTTON_PRESS_L1", "BUTTON_PRESS_R1", "BUTTON_PRESS_L2", "BUTTON_PRESS_R2", "BUTTON_PRESS_S1", "BUTTON_PRESS_S2", "BUTTON_PRESS_A1", "BUTTON_PRESS_A2", "BUTTON_PRESS_L3", "BUTTON_PRESS_R3", "BUTTON_PRESS_FN",
        "BUTTON_PRESS_DDI_UP", "BUTTON_PRESS_DDI_DOWN", "BUTTON_PRESS_DDI_LEFT", "BUTTON_PRESS_DDI_RIGHT", "SUSTAIN_DP_MODE_DP", "SUSTAIN_DP_MODE_LS", "SUSTAIN_DP_MODE_RS", "SUSTAIN_SOCD_MODE_UP_PRIO", "SUSTAIN_SOCD_MODE_NEUTRAL", "SUSTAIN_SOCD_MODE_SECOND_WIN", "SUSTAIN_SOCD_MODE_FIRST_WIN", "SUSTAIN_SOCD_MODE_BYPASS", "BUTTON_PRESS_TURBO", "BUTTON_PRESS_MACRO", "BUTTON_PRESS_MACRO_1", "BUTTON_PRESS_MACRO_2", "BUTTON_PRESS_MACRO_3", "BUTTON_PRESS_MACRO_4", "BUTTON_PRESS_MACRO_5", "BUTTON_PRESS_MACRO_6", "CUSTOM_BUTTON_COMBO", "BUTTON_PRESS_A3", "BUTTON_PRESS_A4",
        "BUTTON_PRESS_E1", "BUTTON_PRESS_E2", "BUTTON_PRESS_E3", "BUTTON_PRESS_E4", "BUTTON_PRESS_E5", "BUTTON_PRESS_E6", "BUTTON_PRESS_E7", "BUTTON_PRESS_E8", "BUTTON_PRESS_E9", "BUTTON_PRESS_E10", "BUTTON_PRESS_E11", "BUTTON_PRESS_E12", "DIGITAL_DIRECTION_UP", "DIGITAL_DIRECTION_DOWN", "DIGITAL_DIRECTION_LEFT", "DIGITAL_DIRECTION_RIGHT", "ANALOG_DIRECTION_LS_X_NEG", "ANALOG_DIRECTION_LS_X_POS", "ANALOG_DIRECTION_LS_Y_NEG", "ANALOG_DIRECTION_LS_Y_POS", "ANALOG_DIRECTION_RS_X_NEG", "ANALOG_DIRECTION_RS_X_POS", "ANALOG_DIRECTION_RS_Y_NEG", "ANALOG_DIRECTION_RS_Y_POS", "ANALOG_DIRECTION_MOD_LOW", "ANALOG_DIRECTION_MOD_HIGH", "BUTTON_PRESS_INPUT_REVERSE", "SUSTAIN_FOCUS_MODE", "SUSTAIN_4_8_WAY_MODE",
        "MENU_NAVIGATION_UP", "MENU_NAVIGATION_DOWN", "MENU_NAVIGATION_LEFT", "MENU_NAVIGATION_RIGHT", "MENU_NAVIGATION_SELECT", "MENU_NAVIGATION_BACK", "MENU_NAVIGATION_TOGGLE", "MODE_GUITAR_FRET_GREEN", "MODE_GUITAR_FRET_RED", "MODE_GUITAR_FRET_YELLOW", "MODE_GUITAR_FRET_BLUE", "MODE_GUITAR_FRET_ORANGE", "MODE_GUITAR_FRET_SOLO_GREEN", "MODE_GUITAR_FRET_SOLO_RED", "MODE_GUITAR_FRET_SOLO_YELLOW", "MODE_GUITAR_FRET_SOLO_BLUE", "MODE_GUITAR_FRET_SOLO_ORANGE", "MODE_GUITAR_WHAMMY", "MODE_GUITAR_PICKUP", "MODE_GUITAR_TILT", "MODE_DRUM_RED_DRUMPAD", "MODE_DRUM_BLUE_DRUMPAD", "MODE_DRUM_YELLOW_DRUMPAD", "MODE_DRUM_GREEN_DRUMPAD", "MODE_DRUM_YELLOW_CYMBAL", "MODE_DRUM_BLUE_CYMBAL", "MODE_DRUM_GREEN_CYMBAL", "MODE_DRUM_KICK_PEDAL_LEFT", "MODE_DRUM_KICK_PEDAL_RIGHT", "MODE_HOTAS_RUDDER_LEFT", "MODE_HOTAS_RUDDER_RIGHT", "MODE_HOTAS_THROTTLE_FORWARD", "MODE_HOTAS_THROTTLE_REVERSE", "MODE_HOTAS_ROCKER_LEFT", "MODE_HOTAS_ROCKER_RIGHT", "MODE_HOTAS_PEDAL_LEFT", "MODE_HOTAS_PEDAL_RIGHT", "MODE_HOTAS_PEDAL_RUDDER_LEFT", "MODE_HOTAS_PEDAL_RUDDER_RIGHT", "MODE_WHEEL_STEERING_LEFT", "MODE_WHEEL_STEERING_RIGHT", "MODE_WHEEL_SHIFTER_GEAR_1", "MODE_WHEEL_SHIFTER_GEAR_2", "MODE_WHEEL_SHIFTER_GEAR_3", "MODE_WHEEL_SHIFTER_GEAR_4", "MODE_WHEEL_SHIFTER_GEAR_5", "MODE_WHEEL_SHIFTER_GEAR_6", "MODE_WHEEL_SHIFTER_GEAR_R", "MODE_WHEEL_SHIFTER_GEAR_N", "MODE_WHEEL_SHIFTER_GEAR_UP", "MODE_WHEEL_SHIFTER_GEAR_DOWN", "MODE_WHEEL_BUTTON_PLUS", "MODE_WHEEL_BUTTON_MINUS", "MODE_WHEEL_DIAL_UP", "MODE_WHEEL_DIAL_DOWN", "MODE_WHEEL_DIAL_ENTER", "MODE_WHEEL_PEDAL_GAS", "MODE_WHEEL_PEDAL_BRAKE", "MODE_WHEEL_PEDAL_CLUTCH"
    }; return v;
}
static Rule number(double min, double max, double initial = 0, bool decimal = false) { Rule r; r.min = min; r.max = max; r.initial = initial; r.decimal = decimal; return r; }
static Rule choice(std::initializer_list<double> values, double initial = 0) { Rule r; for (double n : values) r.choices.push_back(n); r.initial = initial; return r; }
static Rule string(double max, std::string initial = "") { Rule r; r.kind = Kind::String; r.max = max; r.initial = initial; return r; }
Schema schema(const Document& d) {
    Schema s;
    s["Preset"]["format"] = string(64,"GP2040-PSX-PRESET"); s["Preset"]["version"] = choice({3},3);
    s["Profile"]["label"] = string(16,"PSX Profile"); s["Profile"]["source"] = number(1,6,1);
    Rule boolean; boolean.kind = Kind::Boolean; boolean.initial = true; boolean.choices = {false,true}; s["Profile"]["enabled"] = boolean;
    Rule action; action.kind = Kind::Action; action.initial = std::string("NONE"); for (auto& a : actions()) action.choices.push_back(a);
    for (const auto& key : inputs()) { s["Mapping"][key] = action; s["MappingMasks"][key+".buttons"] = number(0,65535); s["MappingMasks"][key+".dpad"] = number(0,15); }
    auto& a = s["Addons"];
    a["analogAdc1Mode"] = choice({1,2},1); a["analogAdc2Mode"] = choice({1,2},2);
    for (auto key : {"analogAdc1Invert","analogAdc2Invert"}) a[key] = choice({0,1,2,3});
    for (auto key : {"negconIMode","negconIIMode","negconLMode"}) a[key] = choice({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14});
    a["negconIMode"].initial = 12.0; a["negconIIMode"].initial = 11.0;
    for (auto key : {"negconIThreshold","negconIIThreshold","negconLThreshold"}) a[key] = number(0,255,128);
    a["negconLThreshold"].initial = 240.0;
    for (auto input : {"I","II","L"}) {
        const auto prefix = std::string("negcon") + input;
        a[prefix+"DigitalEnabled"] = choice({0,1},1);
        a[prefix+"InnerDeadzone"] = number(0,100);
        a[prefix+"OuterDeadzone"] = number(0,100,100);
    }
    a["jogconDeadzone"] = number(0,32766); a["jogconMax"] = number(1,32767,100); a["jogconForcebackEnabled"] = choice({0,1},1);
    for (auto key : {"forced_circularity","forced_circularity2","auto_calibrate","auto_calibrate2","analog_smoothing","analog_smoothing2","BootselButtonAddonEnabled","BoardLedAddonEnabled","ReverseInputEnabled","KeyboardHostAddonEnabled","GamepadUSBHostAddonEnabled","ReactiveLEDAddonEnabled","InputMacroAddonEnabled"}) a[key] = choice({0,1});
    for (auto key : {"inner_deadzone","inner_deadzone2"}) a[key] = number(0,100,5);
    for (auto key : {"outer_deadzone","outer_deadzone2"}) a[key] = number(0,100,100);
    for (auto key : {"smoothing_factor","smoothing_factor2"}) a[key] = number(0,100,5,true);
    for (auto key : {"analog_error","analog_error2"}) a[key] = choice({1000,990,979,969,958,946,934,922,911,900,890,876,863,848,834,821},1000);
    a["onBoardLedMode"] = choice({0,1,2,3});
    for (auto key : {"reverseActionUp","reverseActionDown","reverseActionLeft","reverseActionRight","keyboardHostMouseMovement"}) a[key] = choice({0,1,2});
    for (auto key : {"bootselButtonMap","keyboardHostMouseLeft","keyboardHostMouseMiddle","keyboardHostMouseRight"}) a[key] = number(0,4294967295.0);
    a["keyboardHostMouseSensitivity"] = number(0,100,1,true);
    for (auto key : {"Up","Down","Left","Right","B1","B2","B3","B4","L1","R1","L2","R2","S1","S2","L3","R3","A1","A2","A3","A4","E1","E2","E3","E4","E5","E6","E7","E8","E9","E10","E11","E12","Fn"}) s["KeyboardHostMap"][key] = number(0,255);
    for (int i = 0; i < 3; ++i) {
        auto& r = s["Reactive."+std::to_string(i)]; Rule ra = action; ra.kind = Kind::ReactiveAction; ra.choices.clear();
        for (size_t j = 0; j < actions().size(); ++j) if (j == 0 || (j >= 1 && j <= 18) || (j >= 59 && j <= 66)) ra.choices.push_back(actions()[j]);
        r["action"] = ra; r["modeUp"] = choice({0,1,2,3,4}); r["modeDown"] = choice({0,1,2,3,4}); r["brightness"] = number(0,255,255);
    }
    for (int i = 0; i < 6; ++i) {
        auto name = "Macro."+std::to_string(i); auto& m = s[name];
        m["macroLabel"] = string(64); m["macroType"] = choice({1,2,3},1);
        for (auto key : {"enabled","exclusive","interruptible","showFrames","useMacroTriggerButton"}) m[key] = choice({0,1});
        m["macroTriggerButton"] = number(0,4294967295.0); m["inputCount"] = number(0,30);
        int count = 0; auto section = d.find(name);
        if (section != d.end()) { auto n = section->second.find("inputCount"); if (n != section->second.end()) { validate_value(n->second,m["inputCount"],name+".inputCount"); count = static_cast<int>(std::get<double>(n->second)); } }
        for (int j = 0; j < count; ++j) for (auto key : {"buttonMask","duration","waitDuration"}) s[name+".Input."+std::to_string(j)][key] = number(0,4294967295.0);
    }
    return s;
}
Document defaults() { Document d; for (const auto& section : schema(d)) for (const auto& f : section.second) d[section.first][f.first] = f.second.initial; return d; }
void validate_value(const Value& v, const Rule& r, const std::string& path) {
    if (r.kind == Kind::Number) {
        auto n = std::get_if<double>(&v); if (!n || !std::isfinite(*n) || *n < r.min || *n > r.max || (!r.decimal && std::floor(*n) != *n)) fail(path+": numeric range/type invalid");
    } else if (r.kind == Kind::Boolean) { if (!std::holds_alternative<bool>(v)) fail(path+": bool required"); }
    else {
        auto str = std::get_if<std::string>(&v); if (!str || !valid_utf8(*str)) fail(path+": UTF-8 string required");
        if (r.kind == Kind::String) {
            if (str->size() > r.max || std::any_of(str->begin(),str->end(),[](unsigned char c){return c < 32 || c == 127;})) fail(path+": string length/control character invalid");
            if (path == "Profile.label" && !std::regex_match(*str,std::regex("[a-zA-Z0-9 ]*"))) fail(path+": ASCII letters/digits/spaces only (16 bytes)");
        }
    }
    if (!r.choices.empty() && std::find(r.choices.begin(),r.choices.end(),v) == r.choices.end()) fail(path+": unsupported enum value");
}
static void validate_document(const Document& d, int version) {
    auto s = schema(d);
    if (version == 1 || version == 2) {
        s["Preset"]["version"] = choice({static_cast<double>(version)},version);
        for (auto input : {"I","II","L"}) for (auto suffix : {"DigitalEnabled","InnerDeadzone","OuterDeadzone"})
            s["Addons"].erase(std::string("negcon")+input+suffix);
    }
    if (version == 1) {
        s["Preset"]["version"] = choice({1},1);
        for (auto key : {"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) s["Addons"].erase(key);
    }
    if (d.size() != s.size()) fail("missing/unknown sections");
    for (const auto& section : s) {
        auto it = d.find(section.first); if (it == d.end() || it->second.size() != section.second.size()) fail(section.first+": missing/unknown fields");
        for (const auto& f : section.second) { auto v = it->second.find(f.first); if (v == it->second.end()) fail(section.first+"."+f.first+": required"); validate_value(v->second,f.second,section.first+"."+f.first); }
    }
    if (std::get<std::string>(d.at("Preset").at("format")) != "GP2040-PSX-PRESET") fail("unsupported format");
    auto n = [&](const std::string& sec, const std::string& key) { return std::get<double>(d.at(sec).at(key)); };
    if (version != 1 && n("Addons","jogconDeadzone") >= n("Addons","jogconMax")) fail("Addons: JOGCON deadzone must be less than max");
    for (const auto& key : inputs()) if (std::get<std::string>(d.at("Mapping").at(key)) != "CUSTOM_BUTTON_COMBO" && (n("MappingMasks",key+".buttons") != 0 || n("MappingMasks",key+".dpad") != 0)) fail(key+": masks require CUSTOM_BUTTON_COMBO");
    if (n("Addons","analogAdc1Mode") == n("Addons","analogAdc2Mode") || n("Addons","outer_deadzone") <= n("Addons","inner_deadzone") || n("Addons","outer_deadzone2") <= n("Addons","inner_deadzone2")) fail("Addons: stick outputs/deadzones conflict");
    if (version == 3) for (auto input : {"I","II","L"}) {
        const auto prefix = std::string("negcon")+input;
        if (n("Addons",prefix+"InnerDeadzone") >= n("Addons",prefix+"OuterDeadzone")) fail("Addons: neGcon inner deadzone must be less than outer");
    }
    for (auto key : {"bootselButtonMap","keyboardHostMouseLeft","keyboardHostMouseMiddle","keyboardHostMouseRight"}) {
        auto mask = static_cast<uint32_t>(n("Addons",key)); if (mask && (mask & (mask - 1))) fail(std::string(key)+": single-button mask required");
    }
}
void validate(const Document& d) { validate_document(d, 3); }
Document parse(const std::string& text) {
    if (text.size() > max_bytes || !valid_utf8(text)) fail("TXT exceeds 64KiB or invalid UTF-8");
    for (size_t i=0; i<text.size(); ++i) if (text[i]=='\r' && (i+1==text.size() || text[i+1]!='\n')) fail("bare CR is not allowed");
    Document d; std::string section, line; std::istringstream in(text.substr(text.rfind("\xef\xbb\xbf",0) == 0 ? 3 : 0)); size_t lineNo = 0;
    static const std::regex headerPattern(R"(\[[A-Za-z][A-Za-z0-9.]*\])"), keyPattern("[A-Za-z][A-Za-z0-9_.]*");
    while (std::getline(in,line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find('\r') != std::string::npos) fail("line "+std::to_string(lineNo)+": bare CR is not allowed");
        line = trim(line); if (line.empty() || line.front() == '#') continue;
        try {
            if (std::regex_match(line,headerPattern)) {
                section = line.substr(1,line.size()-2); if (d.count(section)) fail("duplicate section"); d[section] = {}; continue;
            }
            auto eq = line.find('='); if (eq == std::string::npos || section.empty()) fail("entry outside section or invalid syntax");
            auto key = trim(line.substr(0,eq)); if (!std::regex_match(key,keyPattern) || d[section].count(key)) fail("invalid/duplicate key");
            d[section][key] = parse_value(line.substr(eq+1));
        } catch (const std::exception& e) { fail("line "+std::to_string(lineNo)+": "+e.what()); }
    }
    int legacyVersion = 3;
    auto metadata = d.find("Preset");
    if (metadata != d.end()) {
        auto version = metadata->second.find("version");
        if (version != metadata->second.end()) {
            if (version->second == Value(1.0)) legacyVersion = 1;
            else if (version->second == Value(2.0)) legacyVersion = 2;
        }
    }
    // Validate the exact legacy schema before adding only newly introduced fields.
    validate_document(d, legacyVersion);
    if (legacyVersion != 3) {
        auto rules = schema(d);
        if (legacyVersion == 1) for (auto key : {"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) d["Addons"][key] = rules.at("Addons").at(key).initial;
        for (auto input : {"I","II","L"}) for (auto suffix : {"DigitalEnabled","InnerDeadzone","OuterDeadzone"}) {
            const auto key = std::string("negcon")+input+suffix;
            d["Addons"][key] = rules.at("Addons").at(key).initial;
        }
        d["Preset"]["version"] = 3.0;
    }
    validate(d); return d;
}
std::string serialize(const Document& d) {
    validate(d); std::string out = "# GP2040 PSX preset v3: shared ADDON settings apply to all profiles.\n# neGcon pressure adds to existing outputs; legacy full behavior changes. Migration preserves modes, including 14/13.\n";
    for (const auto& section : d) { out += "\n["+section.first+"]\n"; for (const auto& f : section.second) out += f.first+" = "+format_value(f.second)+"\n"; }
    if (out.size() > max_bytes) fail("TXT exceeds 64KiB"); return out;
}
void set(Document& d, const std::string& section, const std::string& key, Value v) {
    auto rules = schema(d); auto& rule = rules.at(section).at(key); validate_value(v,rule,section+"."+key);
    d.at(section).at(key) = v;
    if (section.rfind("Macro.",0) == 0 && key == "inputCount") {
        auto revised = schema(d);
        for (auto it = d.begin(); it != d.end();) { if (it->first.rfind(section+".Input.",0) == 0 && !revised.count(it->first)) it = d.erase(it); else ++it; }
        for (const auto& s : revised) if (s.first.rfind(section+".Input.",0) == 0) for (const auto& f : s.second) if (!d[s.first].count(f.first)) d[s.first][f.first] = f.second.initial;
    }
}
}
