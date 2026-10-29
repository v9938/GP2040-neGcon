// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#pragma once

#include <cstddef>
#include <cstdint>

#pragma pack(push, 1)
struct XE1AJReport {
    uint16_t buttons;
    uint8_t hat;
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t rz;
};
#pragma pack(pop)

static_assert(sizeof(XE1AJReport) == 7, "XE1AJ input report must be seven bytes");
static_assert(offsetof(XE1AJReport, hat) == 2 && offsetof(XE1AJReport, rz) == 6,
              "XE1AJ report layout must match the HID descriptor");

namespace XE1AJ {
constexpr uint16_t JoystickMid = 0x8000;
constexpr uint8_t HatNeutral = 8;

// GamepadState のビット順。SDK 依存を避け、PC 側でも同じ変換を検証する。
// buttons: B1 B2 B3 B4 L1 R1 L2 R2 S1 S2 L3 R3 A1 A2 ...
// dpad: UP=1, DOWN=2, LEFT=4, RIGHT=8
constexpr XE1AJReport MakeReport(uint16_t buttons, uint8_t dpad,
                                 uint16_t lx, uint16_t ly, uint16_t rx, uint16_t ry) {
    uint8_t hat = HatNeutral;
    switch (dpad & 0x0F) {
        case 1: hat = 0; break;
        case 9: hat = 1; break;
        case 8: hat = 2; break;
        case 10: hat = 3; break;
        case 2: hat = 4; break;
        case 6: hat = 5; break;
        case 4: hat = 6; break;
        case 5: hat = 7; break;
        default: break;
    }
    const uint16_t mapped = static_cast<uint16_t>(
        (buttons & 0x1C3F) | ((buttons & 0x0300) >> 2) | ((buttons & 0x00C0) << 2));
    return {mapped, hat, static_cast<uint8_t>(lx >> 8), static_cast<uint8_t>(ly >> 8),
            static_cast<uint8_t>(rx >> 8), static_cast<uint8_t>(ry >> 8)};
}

// USB のボタン値は little-endian。ホストのバイト順に依存しない検証用。
constexpr uint8_t ReportByte(const XE1AJReport &report, std::size_t index) {
    switch (index) {
        case 0: return static_cast<uint8_t>(report.buttons);
        case 1: return static_cast<uint8_t>(report.buttons >> 8);
        case 2: return report.hat;
        case 3: return report.x;
        case 4: return report.y;
        case 5: return report.z;
        case 6: return report.rz;
        default: return 0;
    }
}
}
