// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "analog_preview.h"
#include <algorithm>
#include <cmath>

uint32_t analog_preview_version() { return 3; }

void analog_preview_get_output(const AnalogPreviewState* state, AnalogPreviewOutput* output) {
    if (state && output) *output = state->output;
}

void analog_preview_reset(AnalogPreviewState* state) {
    if (!state) return;
    *state = {};
    for (auto& stick : state->sticks) {
        stick.xCenter = stick.yCenter = 128;
        stick.xEma = stick.yEma = 0.5f;
    }
}

void analog_transform_stick(uint8_t rawX, uint8_t rawY, uint8_t oneAxis,
    const AnalogPreviewStickSettings* settings, AnalogPreviewStickState* state,
    float* outX, float* outY) {
    if (!settings || !state || !outX || !outY) return;
    if (!state->calibrated) {
        if (settings->autoCalibrate) {
            state->xCenter = std::clamp<int>(rawX, 1, 254);
            state->yCenter = oneAxis ? 128 : std::clamp<int>(rawY, 1, 254);
        }
        state->calibrated = 1;
    }
    const auto normalize = [](uint8_t raw, uint8_t center) {
        return raw <= center ? 0.5f * raw / center
            : 0.5f + 0.5f * (raw - center) / (255 - center);
    };
    float x = normalize(rawX, state->xCenter);
    float y = oneAxis ? 0.5f : normalize(rawY, state->yCenter);
    if (settings->invert == 1 || settings->invert == 3) x = 1.0f - x;
    if (!oneAxis && (settings->invert == 2 || settings->invert == 3)) y = 1.0f - y;
    const float alpha = std::isfinite(settings->smoothingFactor)
        ? std::clamp(settings->smoothingFactor / 1000.0f, 0.0f, 1.0f) : 0.0f;
    if (settings->smoothing) {
        x = alpha * x + (1.0f - alpha) * state->xEma;
        if (!oneAxis) y = alpha * y + (1.0f - alpha) * state->yEma;
    }
    state->xEma = x;
    state->yEma = y;
    const float dx = (x - 0.5f) * 2.0f;
    const float dy = oneAxis ? 0.0f : (y - 0.5f) * 2.0f;
    const float radius = oneAxis ? std::fabs(dx) : std::sqrt(dx * dx + dy * dy);
    const float scaledRadius = radius * (settings->error / 1000.0f);
    const float inner = std::clamp(settings->innerDeadzone / 100.0f, 0.0f, 1.0f);
    const float outer = std::clamp(settings->outerDeadzone / 100.0f, 0.0f, 1.0f);
    x = y = 0.5f;
    if (radius != 0.0f && scaledRadius > inner) {
        const float width = outer - inner;
        float scale = width > 0.0f ? (scaledRadius - inner) / width : 1.0f;
        if (oneAxis || settings->circularity) scale = std::min(scale, 1.0f);
        x = std::clamp(0.5f + 0.5f * dx / radius * scale, 0.0f, 1.0f);
        y = oneAxis ? 0.5f : std::clamp(0.5f + 0.5f * dy / radius * scale, 0.0f, 1.0f);
    }
    *outX = x;
    *outY = y;
}

uint16_t analog_preview_convert(float value, uint16_t midpoint) {
    value = std::clamp(value, 0.0f, 1.0f);
    const float output = value <= 0.5f ? value * 2.0f * midpoint
        : midpoint + (value - 0.5f) * 2.0f * (65535u - midpoint);
    return static_cast<uint16_t>(std::lround(output));
}

uint16_t analog_preview_pressure(uint8_t raw, uint32_t mode, uint16_t midpoint) {
    if (mode >= 1 && mode <= 12) {
        const float value = raw / 255.0f;
        const uint32_t range = (mode - 1) % 3;
        return analog_preview_convert(range == 0 ? value
            : range == 1 ? 0.5f + value * 0.5f : 0.5f - value * 0.5f, midpoint);
    }
    return raw;
}

void analog_preview_add_pressures(const uint8_t pressures[3], const uint32_t modes[3],
    const uint32_t inner[3], const uint32_t outer[3], uint16_t mid,
    uint16_t axes[4], uint8_t triggers[2]) {
    if (!pressures || !modes || !inner || !outer || !axes || !triggers) return;
    (void)mid; // 加算幅はドライバの中心値によらず固定。基準値は呼び出し側の既存出力。
    double axisValues[4] = {double(axes[0]), double(axes[1]), double(axes[2]), double(axes[3])};
    double triggerValues[2] = {double(triggers[0]), double(triggers[1])};
    for (unsigned i = 0; i < 3; ++i) {
        if (inner[i] >= outer[i] || outer[i] > 100) continue;
        const double u = std::clamp((pressures[i] / 255.0 * 100.0 - inner[i])
            / (outer[i] - inner[i]), 0.0, 1.0);
        const uint32_t mode = modes[i];
        if (mode >= 1 && mode <= 12) {
            const uint32_t range = (mode - 1) % 3;
            // 全域は押込み0で負の半域、最大で正の半域を既存値へ加算する。
            axisValues[(mode - 1) / 3] += range == 0 ? (u - 0.5) * 65535.0
                : u * (range == 1 ? 32767.5 : -32767.5);
        } else if (mode == 13 || mode == 14) triggerValues[mode - 13] += u * 255.0;
    }
    // 全入力を符号付きで合算してから一度だけ飽和・丸めを行う。
    for (unsigned i = 0; i < 4; ++i)
        axes[i] = static_cast<uint16_t>(std::lround(std::clamp(axisValues[i], 0.0, 65535.0)));
    for (unsigned i = 0; i < 2; ++i)
        triggers[i] = static_cast<uint8_t>(std::lround(std::clamp(triggerValues[i], 0.0, 255.0)));
}

uint16_t analog_preview_jogcon(int32_t position, uint32_t deadzone, uint32_t maximum,
    uint32_t invert, uint16_t midpoint) {
    if (deadzone >= maximum || maximum > 32767) return midpoint;
    // Widen before negation so every int32 input, including INT32_MIN, is safe.
    const int64_t signedPosition = position;
    const uint64_t magnitude = signedPosition < 0 ? -signedPosition : signedPosition;
    if (magnitude <= deadzone) return midpoint;
    const float amount = static_cast<float>(std::min<uint64_t>(magnitude - deadzone,
        maximum - deadzone)) / (maximum - deadzone);
    float value = 0.5f + (position < 0 ? -0.5f : 0.5f) * amount;
    if (invert == 1 || invert == 3) value = 1.0f - value;
    return analog_preview_convert(value, midpoint);
}

void analog_preview_step(const AnalogPreviewInput* input, const AnalogPreviewSettings* settings,
    uint16_t midpoint, uint8_t advance, AnalogPreviewState* state, AnalogPreviewOutput* output) {
    if (!input || !settings || !state || !output) return;
    if (!advance) { analog_preview_get_output(state, output); return; }
    AnalogPreviewOutput next = {midpoint, midpoint, midpoint, midpoint, 0, 0, 0, 0};
    uint16_t* axes[] = {&next.lx, &next.ly, &next.rx, &next.ry};
    const auto offset = [](uint32_t mode) { return mode == 1 ? 0 : mode == 2 ? 2 : -1; };
    const bool active = input->connected && (input->jogcon || input->analog || input->negcon);
    if (active) {
        for (int i = 0; i < (input->jogcon || input->negcon ? 1 : 2); ++i) {
            const int pair = offset(settings->sticks[i].mode);
            if (pair >= 0) next.validityMask |= (input->jogcon || input->negcon ? 1u : 3u) << pair;
        }
        if (!input->jogcon && input->negcon) next.validityMask |= 0x30;
        if (!input->jogcon && input->negcon) for (uint32_t mode : settings->pressureModes) {
            if (mode >= 1 && mode <= 12) next.validityMask |= 1u << ((mode - 1) / 3);
            else if (mode == 13 || mode == 14) next.validityMask |= 1u << (mode - 9);
        }
    }
    const bool changed = state->lastGeneration != input->generation || state->lastId != input->id
        || state->lastConnected != input->connected || state->lastAnalog != input->analog
        || state->lastNegcon != input->negcon || state->lastJogcon != input->jogcon;
    if (changed || !active) {
        analog_preview_reset(state);
        next.reset = 1;
    } else if (input->jogcon) {
        const int pair = offset(settings->sticks[0].mode);
        if (pair >= 0) *axes[pair] = analog_preview_jogcon(input->jogPosition,
            settings->jogconDeadzone, settings->jogconMax, settings->sticks[0].invert, midpoint);
    } else {
        for (int i = 0; i < (input->negcon ? 1 : 2); ++i) {
            const int pair = offset(settings->sticks[i].mode);
            if (pair < 0) continue;
            float x, y;
            analog_transform_stick(i == 0 ? input->lx : input->rx,
                i == 0 ? input->ly : input->ry, input->negcon, &settings->sticks[i],
                &state->sticks[i], &x, &y);
            *axes[pair] = analog_preview_convert(x, midpoint);
            if (!input->negcon) *axes[pair + 1] = analog_preview_convert(y, midpoint);
        }
        if (input->negcon) {
            const uint8_t pressures[] = {input->pressureI, input->pressureII, input->pressureL};
            uint16_t values[] = {next.lx, next.ly, next.rx, next.ry};
            uint8_t triggers[] = {next.lt, next.rt};
            analog_preview_add_pressures(pressures, settings->pressureModes,
                settings->pressureInner, settings->pressureOuter, midpoint, values, triggers);
            next.lx = values[0]; next.ly = values[1]; next.rx = values[2]; next.ry = values[3];
            next.lt = triggers[0]; next.rt = triggers[1];
        }
    }
    state->lastGeneration = input->generation;
    state->lastId = input->id;
    state->lastConnected = input->connected;
    state->lastAnalog = input->analog;
    state->lastNegcon = input->negcon;
    state->lastJogcon = input->jogcon;
    state->output = next;
    *output = next;
}
