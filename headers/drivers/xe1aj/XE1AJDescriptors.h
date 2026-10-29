// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#pragma once

#include <cstdint>

// 基本 HID のみ。Vendor Defined、Feature、Report ID は含めない。
inline constexpr uint8_t xe1aj_report_descriptor[] = {
    0x05, 0x01,       // Generic Desktop
    0x09, 0x05,       // Game Pad
    0xA1, 0x01,       // Application Collection
    0x05, 0x09,       // Button
    0x19, 0x01,
    0x29, 0x0D,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x0D,
    0x81, 0x02,       // 13 buttons
    0x75, 0x03,
    0x95, 0x01,
    0x81, 0x03,       // 3-bit constant padding
    0x05, 0x01,
    0x09, 0x39,       // Hat Switch
    0x15, 0x00,
    0x25, 0x07,
    0x35, 0x00,
    0x46, 0x3B, 0x01, // Physical maximum 315 degrees
    0x65, 0x14,       // Angular position, degrees
    0x75, 0x04,
    0x95, 0x01,
    0x81, 0x42,       // Null state: 8
    0x81, 0x03,       // 4-bit constant padding
    0x65, 0x00,
    0x35, 0x00,
    0x45, 0x00,       // Reset physical range for axes
    0x09, 0x30,       // X
    0x09, 0x31,       // Y
    0x09, 0x32,       // Z
    0x09, 0x35,       // Rz
    0x15, 0x00,
    0x26, 0xFF, 0x00, // Unsigned logical maximum 255
    0x75, 0x08,
    0x95, 0x04,
    0x81, 0x02,
    0xC0,
};

inline constexpr uint8_t xe1aj_device_descriptor[] = {
    18, 0x01, 0x00, 0x02, // USB 2.00
    0x00, 0x00, 0x00, 64, // EP0 は TinyUSB の共通設定 64 に合わせる。
    0xA7, 0x17, 0x05, 0x00,
    0x01, 0x00,           // Device 0.01
    1, 2, 3, 1,
};

inline constexpr uint8_t xe1aj_configuration_descriptor[] = {
    9, 0x02, 34, 0, 1, 1, 0, 0xA0, 50,
    9, 0x04, 0, 0, 1, 0x03, 0, 0, 0,
    9, 0x21, 0x11, 0x01, 0, 1, 0x22,
    static_cast<uint8_t>(sizeof(xe1aj_report_descriptor)),
    static_cast<uint8_t>(sizeof(xe1aj_report_descriptor) >> 8),
    7, 0x05, 0x81, 0x03, 64, 0, 1,
};

inline constexpr const char *xe1aj_string_descriptors[] = {
    nullptr, "GP2040-CE", "XE1AJ-USB", "1.0",
};

static_assert(sizeof(xe1aj_device_descriptor) == 18, "Invalid device descriptor length");
static_assert(sizeof(xe1aj_configuration_descriptor) == 34, "Invalid configuration length");
