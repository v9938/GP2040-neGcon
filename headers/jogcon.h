// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#ifndef JOGCON_H
#define JOGCON_H

#include <cstdint>

struct JogconSettings {
    uint32_t deadzone = 0, max = 100;
    uint8_t forceback = 1;
};

enum class JogconResetStatus : uint8_t {
    disconnected = 0, idle = 1, returning = 2, complete = 3, timeout = 4, fault = 5
};

struct JogconState {
    int32_t position = 0;
    uint32_t stableSince = 0, startTime = 0, lastValidTime = 0;
    uint8_t motorCommand = 0, motorPower = 0;
    JogconResetStatus resetStatus = JogconResetStatus::disconnected;
    uint8_t connected = 0, ready = 0, stable = 0, hasValid = 0, status = 0;
};

// 候補プロトコルの4bit強度上限。実機での安全性と指令方向は未検証。
constexpr uint8_t JOGCON_MAX_POWER = 15;
constexpr uint32_t JOGCON_STALE_MS = 100;
constexpr uint32_t JOGCON_RESET_TIMEOUT_MS = 3000;
constexpr uint32_t JOGCON_STABLE_MS = 100;
constexpr uint32_t JOGCON_STABLE_COUNTS = 4;

// reset keeps the RAW zero origin; it never subtracts a software offset.
void jogcon_reset(JogconState& state, uint32_t nowMs);
void jogcon_step(JogconState& state, int32_t position, uint8_t status,
    bool connected, bool ready, const JogconSettings& settings, uint32_t nowMs);
// Called between valid packets; it must not refresh lastValidTime.
void jogcon_guard(JogconState& state, bool connected, bool ready, uint32_t nowMs);

#if defined(_WIN32) && defined(ANALOG_PREVIEW_DLL)
#define JOGCON_MODEL_API __declspec(dllexport)
#else
#define JOGCON_MODEL_API
#endif
extern "C" {
JOGCON_MODEL_API uint32_t jogcon_model_version();
JOGCON_MODEL_API void jogcon_model_reset(JogconState* state, uint32_t nowMs);
JOGCON_MODEL_API void jogcon_model_step(JogconState* state, int32_t position,
    uint8_t status, uint8_t connected, uint8_t ready, const JogconSettings* settings, uint32_t nowMs);
JOGCON_MODEL_API void jogcon_model_guard(JogconState* state, uint8_t connected, uint8_t ready, uint32_t nowMs);
}

static_assert(sizeof(JogconSettings) == 12, "Jogcon ABI v1");
static_assert(sizeof(JogconState) == 24, "Jogcon ABI v1");
#endif
