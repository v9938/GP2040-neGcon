// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "drivers/xe1aj/XE1AJDriver.h"
#include "storagemanager.h"

#include <cstring>

static_assert(CFG_TUD_ENDPOINT0_SIZE == 64, "XE1AJ descriptor requires the global 64-byte EP0");
static_assert(GAMEPAD_MASK_UP == 1 && GAMEPAD_MASK_DOWN == 2 &&
              GAMEPAD_MASK_LEFT == 4 && GAMEPAD_MASK_RIGHT == 8, "Unexpected dpad bits");
static_assert(GAMEPAD_MASK_B1 == 1 && GAMEPAD_MASK_B2 == 2 && GAMEPAD_MASK_B3 == 4 &&
              GAMEPAD_MASK_B4 == 8 && GAMEPAD_MASK_L1 == 16 && GAMEPAD_MASK_R1 == 32 &&
              GAMEPAD_MASK_L2 == 64 && GAMEPAD_MASK_R2 == 128 && GAMEPAD_MASK_S1 == 256 &&
              GAMEPAD_MASK_S2 == 512 && GAMEPAD_MASK_L3 == 1024 && GAMEPAD_MASK_R3 == 2048 &&
              GAMEPAD_MASK_A1 == 4096, "Unexpected GamepadState button bits");

namespace {
uint8_t idle_rate = 0;
bool force_report = true;

void xe1aj_hid_reset(uint8_t rhport) {
    idle_rate = 0;
    force_report = true;
    hidd_reset(rhport);
}

bool xe1aj_hid_control(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
        if (request->wIndex != 0 || request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE)
            return false;
        switch (request->bRequest) {
            case HID_REQ_CONTROL_GET_REPORT:
                if (request->bmRequestType_bit.direction != TUSB_DIR_IN ||
                    request->wValue != (HID_REPORT_TYPE_INPUT << 8))
                    return false;
                break;
            case HID_REQ_CONTROL_SET_IDLE:
                if (request->bmRequestType_bit.direction != TUSB_DIR_OUT ||
                    (request->wValue & 0xFF) != 0 || request->wLength != 0)
                    return false;
                break;
            case HID_REQ_CONTROL_GET_IDLE:
                if (request->bmRequestType_bit.direction != TUSB_DIR_IN || request->wValue != 0)
                    return false;
                break;
            default:
                return false;
        }
    }
    const bool accepted = hidd_control_xfer_cb(rhport, stage, request);
    if (accepted && stage == CONTROL_STAGE_SETUP &&
        request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bRequest == HID_REQ_CONTROL_SET_IDLE) {
        idle_rate = static_cast<uint8_t>(request->wValue >> 8);
        force_report = true;
    }
    return accepted;
}
}

void XE1AJDriver::initialize() {
    report = XE1AJ::MakeReport(0, 0, XE1AJ::JoystickMid, XE1AJ::JoystickMid,
                              XE1AJ::JoystickMid, XE1AJ::JoystickMid);
    std::memset(last_report, 0, sizeof(last_report));
    last_sent_ms = 0;
    idle_rate = 0;
    force_report = true;
    class_driver = {};
    class_driver.name = "XE1AJ";
    class_driver.init = hidd_init;
    class_driver.deinit = hidd_deinit;
    class_driver.reset = xe1aj_hid_reset;
    class_driver.open = hidd_open;
    class_driver.control_xfer_cb = xe1aj_hid_control;
    class_driver.xfer_cb = hidd_xfer_cb;
}

bool XE1AJDriver::process(Gamepad *gamepad) {
    report = XE1AJ::MakeReport(static_cast<uint16_t>(gamepad->state.buttons), gamepad->state.dpad,
                              gamepad->state.lx, gamepad->state.ly, gamepad->state.rx, gamepad->state.ry);
    if (tud_suspended())
        tud_remote_wakeup();

    const uint32_t now = to_ms_since_boot(get_absolute_time());
    // TinyUSB は IDLE 値を保持するだけなので、期限による再送はここで行う。
    const bool idle_due = idle_rate != 0 && now - last_sent_ms >= static_cast<uint32_t>(idle_rate) * 4;
    if (force_report || idle_due || std::memcmp(last_report, &report, sizeof(report)) != 0) {
        if (tud_hid_ready() && tud_hid_report(0, &report, sizeof(report))) {
            std::memcpy(last_report, &report, sizeof(report));
            last_sent_ms = now;
            force_report = false;
            return true;
        }
    }
    return false;
}

uint16_t XE1AJDriver::get_report(uint8_t report_id, hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    if (report_id != 0 || report_type != HID_REPORT_TYPE_INPUT || buffer == nullptr)
        return 0;
    const uint16_t length = reqlen < sizeof(report) ? reqlen : sizeof(report);
    std::memcpy(buffer, &report, length);
    return length;
}

const uint16_t *XE1AJDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
    static const uint16_t languages[] = {0x0304, 0x0409};
    if (index == 0)
        return languages;
    if (index >= sizeof(xe1aj_string_descriptors) / sizeof(xe1aj_string_descriptors[0]) ||
        (langid != 0 && langid != 0x0409))
        return nullptr;

    const auto &options = Storage::getInstance().getGamepadOptions();
    const char *value = xe1aj_string_descriptors[index];
    size_t capacity = 126;
    if (options.usbDescOverride) {
        switch (index) {
            case 1: value = options.usbDescManufacturer; capacity = sizeof(options.usbDescManufacturer); break;
            case 2: value = options.usbDescProduct; capacity = sizeof(options.usbDescProduct); break;
            case 3: value = options.usbDescVersion; capacity = sizeof(options.usbDescVersion); break;
        }
    }
    static uint16_t descriptor[127];
    size_t length = 0;
    while (length < capacity && length < 126 && value[length] != '\0') {
        descriptor[length + 1] = static_cast<uint8_t>(value[length]);
        ++length;
    }
    descriptor[0] = static_cast<uint16_t>(0x0300 | (2 * length + 2));
    return descriptor;
}

const uint8_t *XE1AJDriver::get_descriptor_device_cb() {
    const auto &options = Storage::getInstance().getGamepadOptions();
    if (!options.usbOverrideID)
        return xe1aj_device_descriptor;
    static uint8_t descriptor[sizeof(xe1aj_device_descriptor)];
    std::memcpy(descriptor, xe1aj_device_descriptor, sizeof(descriptor));
    descriptor[8] = static_cast<uint8_t>(options.usbVendorID);
    descriptor[9] = static_cast<uint8_t>(options.usbVendorID >> 8);
    descriptor[10] = static_cast<uint8_t>(options.usbProductID);
    descriptor[11] = static_cast<uint8_t>(options.usbProductID >> 8);
    return descriptor;
}

const uint8_t *XE1AJDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return itf == 0 ? xe1aj_report_descriptor : nullptr;
}

const uint8_t *XE1AJDriver::get_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? xe1aj_configuration_descriptor : nullptr;
}
