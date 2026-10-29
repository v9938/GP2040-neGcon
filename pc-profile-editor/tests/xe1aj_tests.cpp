// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "drivers/xe1aj/XE1AJReport.h"
#include "drivers/xe1aj/XE1AJDescriptors.h"
#include <cassert>

int main() {
    // 保存済みGamepadのボタン順をUSB Button 1～13へ変換する。
    constexpr unsigned inputs[] = {0,1,2,3,4,5,8,9,6,7,10,11,12};
    for (unsigned output = 0; output < 13; ++output) {
        const auto report = XE1AJ::MakeReport(1u << inputs[output], 0, 0x8000, 0x8000, 0x8000, 0x8000);
        assert(report.buttons == (1u << output));
    }
    constexpr unsigned directions[] = {1,9,8,10,2,6,4,5};
    for (unsigned hat = 0; hat < 8; ++hat)
        assert(XE1AJ::MakeReport(0, directions[hat], 0, 0, 0, 0).hat == hat);
    const auto neutral = XE1AJ::MakeReport(0, 0, 0x8000, 0x8000, 0x8000, 0x8000);
    const unsigned expected[] = {0,0,8,128,128,128,128};
    for (unsigned i = 0; i < 7; ++i) assert(XE1AJ::ReportByte(neutral,i) == expected[i]);
    const auto limits = XE1AJ::MakeReport(0xffff, 3, 0, 65535, 65535, 0);
    assert(limits.buttons == 0x1fff && limits.hat == 8);
    assert(limits.x == 0 && limits.y == 255 && limits.z == 255 && limits.rz == 0);
    assert(xe1aj_configuration_descriptor[4] == 1);
    assert(xe1aj_device_descriptor[8] == 0xa7 && xe1aj_device_descriptor[9] == 0x17);
    assert(xe1aj_device_descriptor[10] == 5);
    unsigned offset = 0;
    for (unsigned i = 0; i < sizeof(xe1aj_report_descriptor);) {
        const unsigned prefix = xe1aj_report_descriptor[i++];
        const unsigned size = (prefix & 3) == 3 ? 4 : prefix & 3;
        assert(prefix != 0x85); // Report IDなし
        if (prefix == 0x95) offset += xe1aj_report_descriptor[i];
        i += size;
    }
    assert(offset > 0);
}
