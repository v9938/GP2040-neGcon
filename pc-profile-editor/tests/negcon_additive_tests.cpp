// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "analog_preview.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

static_assert(sizeof(AnalogPreviewSettings) == 104, "ABI v3 settings");
static_assert(sizeof(AnalogPreviewInput) == 24 && sizeof(AnalogPreviewState) == 48
    && sizeof(AnalogPreviewOutput) == 12, "ABI v3 unchanged layouts");
static_assert(offsetof(AnalogPreviewSettings, jogconDeadzone) == 92
    && offsetof(AnalogPreviewSettings, jogconMax) == 96
    && offsetof(AnalogPreviewSettings, jogconForcebackEnabled) == 100
    && offsetof(AnalogPreviewSettings, pressureInner) == 68
    && offsetof(AnalogPreviewSettings, pressureOuter) == 80, "ABI v3 offsets");

int main() {
    assert(analog_preview_version() == 3);
    const uint32_t inner[] = {0, 0, 0}, outer[] = {100, 100, 100};
    unsigned cases = 0;
    const auto check = [&](std::array<uint8_t, 3> pressures, std::array<uint32_t, 3> modes,
        std::array<uint16_t, 4> base, std::array<uint16_t, 4> expected,
        std::array<uint8_t, 2> triggers = {0, 0}, std::array<uint8_t, 2> expectedTriggers = {0, 0}) {
        analog_preview_add_pressures(pressures.data(), modes.data(), inner, outer, 32768,
            base.data(), triggers.data());
        assert(base == expected && triggers == expectedTriggers);
        ++cases;
    };
    const std::array<uint16_t, 4> center = {32768, 32768, 32768, 32768};
    check({255, 0, 0}, {1, 0, 0}, center, {65535, 32768, 32768, 32768});
    check({0, 0, 0}, {1, 0, 0}, center, {1, 32768, 32768, 32768});
    check({128, 0, 0}, {1, 0, 0}, center, {32897, 32768, 32768, 32768});
    check({0, 0, 0}, {1, 0, 0}, {10000, 32768, 32768, 32768}, {0, 32768, 32768, 32768});
    check({255, 0, 0}, {1, 0, 0}, {10000, 32768, 32768, 32768}, {42768, 32768, 32768, 32768});
    check({255, 0, 0}, {2, 0, 0}, center, {65535, 32768, 32768, 32768});
    // Fixed half-span is 32767.5: 32768 - 32767.5 rounds once to 1.
    check({255, 0, 0}, {3, 0, 0}, center, {1, 32768, 32768, 32768});
    check({255, 0, 0}, {3, 0, 0}, {32767, 32768, 32768, 32768}, {0, 32768, 32768, 32768});
    for (uint8_t raw : {uint8_t(1), uint8_t(128), uint8_t(255)}) {
        check({raw, raw, 0}, {12, 11, 0}, center, center);
        check({raw, raw, 0}, {11, 12, 0}, center, center);
    }
    check({255, 255, 255}, {1, 3, 3}, center, {1, 32768, 32768, 32768});
    check({255, 255, 255}, {3, 1, 3}, center, {1, 32768, 32768, 32768});
    check({255, 255, 255}, {3, 3, 1}, center, {1, 32768, 32768, 32768});
    check({255, 255, 0}, {2, 3, 0}, {65535, 32768, 32768, 32768}, {65535, 32768, 32768, 32768});
    check({255, 0, 0}, {2, 0, 0}, {65535, 32768, 32768, 32768}, {65535, 32768, 32768, 32768});
    check({255, 255, 255}, {3, 3, 3}, {0, 32768, 32768, 32768}, {0, 32768, 32768, 32768});
    check({100, 0, 0}, {13, 0, 0}, center, center, {100, 7}, {200, 7});
    check({200, 100, 0}, {13, 13, 0}, center, center, {100, 7}, {255, 7});
    check({255, 255, 255}, {14, 14, 14}, center, center, {8, 0}, {8, 255});
    check({255, 255, 255}, {0, 15, std::numeric_limits<uint32_t>::max()}, center, center);

    // Compare every destination against an independent double-precision reference.
    for (uint32_t mode = 1; mode <= 14; ++mode) {
        for (uint8_t raw : {uint8_t(0), uint8_t(25), uint8_t(26), uint8_t(128), uint8_t(204), uint8_t(255)}) {
            const uint8_t pressures[] = {raw, 0, 0};
            const uint32_t modes[] = {mode, 0, 0}, dzInner[] = {10, 0, 0}, dzOuter[] = {80, 100, 100};
            auto axes = center;
            std::array<uint8_t, 2> triggers = {31, 17};
            analog_preview_add_pressures(pressures, modes, dzInner, dzOuter, 32768, axes.data(), triggers.data());
            const double u = std::clamp((double(raw) / 255.0 * 100.0 - 10.0) / 70.0, 0.0, 1.0);
            if (mode <= 12) {
                const uint32_t range = (mode - 1) % 3;
                const double delta = range == 0 ? (u - 0.5) * 65535.0
                    : u * (range == 1 ? 32767.5 : -32767.5);
                auto expected = center;
                expected[(mode - 1) / 3] = uint16_t(std::lround(std::clamp(32768.0 + delta, 0.0, 65535.0)));
                assert(axes == expected && triggers[0] == 31 && triggers[1] == 17);
            } else {
                const unsigned target = mode - 13;
                assert(axes == center);
                assert(triggers[target] == uint8_t(std::lround(std::min(255.0, (target == 0 ? 31.0 : 17.0) + u * 255.0))));
            }
            ++cases;
        }
    }
    for (const auto dz : {std::array<uint32_t, 2>{100, 100}, {80, 10}, {0, 101},
                         {std::numeric_limits<uint32_t>::max(), 100}}) {
        auto axes = center;
        uint8_t triggers[] = {255, 255};
        const uint8_t pressures[] = {255, 255, 255};
        const uint32_t modes[] = {1, 1, 1}, invalidInner[] = {dz[0], dz[0], dz[0]}, invalidOuter[] = {dz[1], dz[1], dz[1]};
        analog_preview_add_pressures(pressures, modes, invalidInner, invalidOuter, 32768, axes.data(), triggers);
        assert(axes == center && triggers[0] == 255 && triggers[1] == 255);
        ++cases;
    }

    AnalogPreviewSettings settings{};
    settings.sticks[0].mode = 1;
    settings.sticks[1].mode = 2;
    for (auto& stick : settings.sticks) { stick.outerDeadzone = 100; stick.error = 1000; }
    settings.pressureModes[0] = 12;
    settings.pressureModes[1] = 11;
    std::copy(std::begin(outer), std::end(outer), settings.pressureOuter);
    AnalogPreviewInput input{};
    input.generation = 1;
    input.connected = input.negcon = 1;
    input.id = 0x23;
    input.lx = input.ly = input.rx = input.ry = 128;
    AnalogPreviewState state{};
    AnalogPreviewOutput output{};
    analog_preview_reset(&state);
    analog_preview_step(&input, &settings, 32768, 1, &state, &output);
    assert(output.reset && output.lx == 32768 && output.ry == 32768);
    input.pressureI = input.pressureII = 255;
    analog_preview_step(&input, &settings, 32768, 1, &state, &output);
    assert(!output.reset && output.ry == 32768 && output.lx == 32768);
    assert(output.validityMask == 0x39 && output.lt == 0 && output.rt == 0);
    input.lx = 255;
    settings.pressureModes[0] = 2;
    settings.pressureModes[1] = 3;
    analog_preview_step(&input, &settings, 32768, 1, &state, &output);
    assert(output.lx == 65535);
    input.pressureI = input.pressureII = 0;
    input.lx = 128;
    analog_preview_step(&input, &settings, 32768, 1, &state, &output);
    assert(output.lx == 32768);
    const auto cached = output;
    input.lx = 0;
    analog_preview_step(&input, &settings, 32768, 0, &state, &output);
    assert(output.lx == cached.lx);
    input.connected = 0;
    analog_preview_step(&input, &settings, 32768, 1, &state, &output);
    assert(output.reset && output.validityMask == 0 && output.lx == 32768);
    std::cout << "neGcon additive reference cases: " << cases << "; ABI v3 and preview transitions PASS\n";
}
