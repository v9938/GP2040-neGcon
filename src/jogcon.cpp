// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "jogcon.h"
#include <algorithm>

static void stopMotor(JogconState& state) {
    state.motorCommand = 0;
    state.motorPower = 0;
}

void jogcon_reset(JogconState& state, uint32_t nowMs) {
    stopMotor(state);
    state.startTime = nowMs;
    state.stableSince = nowMs;
    state.stable = 0;
    state.hasValid = 0;
    state.lastValidTime = nowMs;
    state.resetStatus = JogconResetStatus::returning;
}

void jogcon_guard(JogconState& state, bool connected, bool ready, uint32_t nowMs) {
    const bool newConnection = connected && !state.connected;
    state.connected = connected;
    state.ready = ready;
    if (!ready) state.stable = 0;
    if (!connected) {
        state = JogconState{};
        return;
    }
    if (newConnection) jogcon_reset(state, nowMs);
    if (state.resetStatus == JogconResetStatus::returning &&
        uint32_t(nowMs - state.startTime) >= JOGCON_RESET_TIMEOUT_MS) {
        state.resetStatus = JogconResetStatus::timeout;
        state.stable = 0;
    }
    if (state.hasValid && uint32_t(nowMs - state.lastValidTime) >= JOGCON_STALE_MS) {
        if (state.resetStatus != JogconResetStatus::timeout)
            state.resetStatus = JogconResetStatus::fault;
        state.stable = 0;
    }
    if (!ready || !state.hasValid || state.resetStatus == JogconResetStatus::timeout ||
        state.resetStatus == JogconResetStatus::fault)
        stopMotor(state);
}

void jogcon_step(JogconState& state, int32_t position, uint8_t status,
    bool connected, bool ready, const JogconSettings& settings, uint32_t nowMs) {
    // Check the previous sample before accepting this one: stale gaps cannot auto-retry.
    jogcon_guard(state, connected, ready, nowMs);
    if (!connected) return;
    const int32_t previousPosition = state.position;
    state.position = position;
    state.status = status;
    state.lastValidTime = nowMs;
    state.hasValid = 1;
    stopMotor(state);
    // 0=停止、1/2=回転方向。4を含む未知の下位ステータスは安全停止する。
    if ((status & 0x0f) > 2) {
        state.resetStatus = JogconResetStatus::fault;
        state.stable = 0;
    }
    if (!ready || state.resetStatus == JogconResetStatus::timeout ||
        state.resetStatus == JogconResetStatus::fault) return;

    const uint64_t magnitude = position < 0 ? uint64_t(-int64_t(position)) : uint64_t(position);
    // 完了直後の惰性で許容幅を外れた場合も、元の3秒期限内だけ再調整する。
    if (state.resetStatus == JogconResetStatus::complete && magnitude > JOGCON_STABLE_COUNTS &&
        uint32_t(nowMs - state.stableSince) < JOGCON_STABLE_MS + 200 &&
        uint32_t(nowMs - state.startTime) < JOGCON_RESET_TIMEOUT_MS) {
        state.resetStatus = JogconResetStatus::returning;
        state.stable = 0;
    }
    uint64_t power = 0;
    if (state.resetStatus == JogconResetStatus::returning) {
        if (magnitude <= JOGCON_STABLE_COUNTS && previousPosition == position && (status & 0x0f) == 0) {
            if (!state.stable) {
                state.stable = 1;
                state.stableSince = nowMs;
            }
            if (uint32_t(nowMs - state.stableSince) >= JOGCON_STABLE_MS)
                state.resetStatus = JogconResetStatus::complete;
            return;
        }
        state.stable = 0;
        // 中央へ移動中は先に駆動を止め、惰性による行き過ぎを抑える。
        const uint64_t previousMagnitude = previousPosition < 0 ? uint64_t(-int64_t(previousPosition)) : uint64_t(previousPosition);
        if (magnitude <= JOGCON_STABLE_COUNTS || (magnitude < 32 && magnitude < previousMagnitude)) return;
        power = magnitude < 32 ? magnitude / 8 + 1 : magnitude / 2 + 1;
    } else if (settings.forceback && magnitude > settings.max) {
        const uint64_t excess = magnitude - settings.max;
        power = (excess + 1) / 2;
    }
    if (!power) return;
    state.motorPower = static_cast<uint8_t>(std::min<uint64_t>(JOGCON_MAX_POWER, power));
    // 実機未検証の候補プロトコル。正位置は負方向、負位置は正方向へ駆動する。
    state.motorCommand = static_cast<uint8_t>((position > 0 ? 0x20 : 0x10) | state.motorPower);
}

extern "C" {
uint32_t jogcon_model_version() { return 1; }
void jogcon_model_reset(JogconState* state, uint32_t nowMs) {
    if (state) jogcon_reset(*state, nowMs);
}
void jogcon_model_step(JogconState* state, int32_t position, uint8_t status,
    uint8_t connected, uint8_t ready, const JogconSettings* settings, uint32_t nowMs) {
    if (state && settings) jogcon_step(*state, position, status, connected != 0, ready != 0, *settings, nowMs);
}
void jogcon_model_guard(JogconState* state, uint8_t connected, uint8_t ready, uint32_t nowMs) {
    if (state) jogcon_guard(*state, connected != 0, ready != 0, nowMs);
}
}
