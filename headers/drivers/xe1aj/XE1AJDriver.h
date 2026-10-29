// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#pragma once

#include "gpdriver.h"
#include "drivers/xe1aj/XE1AJDescriptors.h"
#include "drivers/xe1aj/XE1AJReport.h"

class XE1AJDriver : public GPDriver {
public:
    void initialize() override;
    bool process(Gamepad *gamepad) override;
    void initializeAux() override {}
    void processAux() override {}
    uint16_t get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) override;
    void set_report(uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer, uint16_t bufsize) override {}
    bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) override { return false; }
    const uint16_t *get_descriptor_string_cb(uint8_t index, uint16_t langid) override;
    const uint8_t *get_descriptor_device_cb() override;
    const uint8_t *get_hid_descriptor_report_cb(uint8_t itf) override;
    const uint8_t *get_descriptor_configuration_cb(uint8_t index) override;
    const uint8_t *get_descriptor_device_qualifier_cb() override { return nullptr; }
    uint16_t GetJoystickMidValue() override { return XE1AJ::JoystickMid; }
    USBListener *get_usb_auth_listener() override { return nullptr; }

private:
    XE1AJReport report = XE1AJ::MakeReport(0, 0, 0x8000, 0x8000, 0x8000, 0x8000);
    uint8_t last_report[sizeof(XE1AJReport)] = {};
    uint32_t last_sent_ms = 0;
};
