// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#ifndef ANALOG_PREVIEW_H
#define ANALOG_PREVIEW_H

#include <cstdint>

// ABI v3: natural alignment, uint8_t flags (never bool), no firmware dependencies.
// Units match storage: deadzones percent, smoothing/error per mille.
struct AnalogPreviewStickSettings {
    uint32_t mode, invert, innerDeadzone, outerDeadzone;
    float smoothingFactor;
    uint32_t error;
    uint8_t smoothing, circularity, autoCalibrate, reserved;
};
struct AnalogPreviewSettings {
    AnalogPreviewStickSettings sticks[2];
    uint32_t pressureModes[3]; // I, II, L: 0 disabled, 1..12 axes, 13 LT, 14 RT.
    uint32_t pressureInner[3], pressureOuter[3];
    uint32_t jogconDeadzone, jogconMax;
    uint8_t jogconForcebackEnabled, reserved[3];
};
struct AnalogPreviewInput {
    uint32_t generation;
    uint8_t lx, ly, rx, ry, pressureI, pressureII, pressureL;
    uint8_t connected, analog, negcon, id, reserved;
    int32_t jogPosition;
    uint8_t jogcon, jogStatus, jogReady, jogReserved;
};
struct AnalogPreviewStickState {
    float xEma, yEma;
    uint8_t xCenter, yCenter, calibrated, reserved;
};
struct AnalogPreviewOutput {
    uint16_t lx, ly, rx, ry;
    uint8_t lt, rt, validityMask, reset;
};
struct AnalogPreviewState {
    AnalogPreviewStickState sticks[2];
    uint32_t lastGeneration;
    uint8_t lastId, lastConnected, lastAnalog, lastNegcon;
    AnalogPreviewOutput output;
    uint8_t lastJogcon, reserved[3];
};

#if defined(_WIN32) && defined(ANALOG_PREVIEW_DLL)
#define ANALOG_PREVIEW_API __declspec(dllexport)
#else
#define ANALOG_PREVIEW_API
#endif
extern "C" {
ANALOG_PREVIEW_API uint32_t analog_preview_version();
// Reset before first step, and whenever settings or the output midpoint change.
ANALOG_PREVIEW_API void analog_preview_reset(AnalogPreviewState* state);
// advance=1 applies ONE recurrence. Preview callers schedule one per virtual ms;
// normal runtime uses the same stick math once per original process loop.
// advance=0 reads the cached result without recalibration or EMA updates.
ANALOG_PREVIEW_API void analog_preview_step(const AnalogPreviewInput* input,
    const AnalogPreviewSettings* settings, uint16_t midpoint, uint8_t advance,
    AnalogPreviewState* state, AnalogPreviewOutput* output);
ANALOG_PREVIEW_API void analog_preview_get_output(const AnalogPreviewState* state, AnalogPreviewOutput* output);
ANALOG_PREVIEW_API void analog_transform_stick(uint8_t rawX, uint8_t rawY,
    uint8_t oneAxis, const AnalogPreviewStickSettings* settings,
    AnalogPreviewStickState* state, float* x, float* y);
ANALOG_PREVIEW_API uint16_t analog_preview_convert(float value, uint16_t midpoint);
ANALOG_PREVIEW_API uint16_t analog_preview_pressure(uint8_t raw, uint32_t mode, uint16_t midpoint);
ANALOG_PREVIEW_API void analog_preview_add_pressures(const uint8_t pressures[3],
    const uint32_t modes[3], const uint32_t inner[3], const uint32_t outer[3],
    uint16_t mid, uint16_t axes[4], uint8_t triggers[2]);
ANALOG_PREVIEW_API uint16_t analog_preview_jogcon(int32_t position, uint32_t deadzone,
    uint32_t maximum, uint32_t invert, uint16_t midpoint);
}

static_assert(sizeof(AnalogPreviewStickSettings) == 28, "ABI v3");
static_assert(sizeof(AnalogPreviewSettings) == 104, "ABI v3");
static_assert(sizeof(AnalogPreviewInput) == 24, "ABI v3");
static_assert(sizeof(AnalogPreviewStickState) == 12, "ABI v3");
static_assert(sizeof(AnalogPreviewOutput) == 12, "ABI v3");
static_assert(sizeof(AnalogPreviewState) == 48, "ABI v3");
#endif
