// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "jogcon.h"
#include <cassert>
#include <climits>

int main() {
    JogconSettings settings;
    JogconState state;
    jogcon_step(state, 50, 0, true, true, settings, 10);
    jogcon_reset(state, 10);
    jogcon_step(state, 50, 1, true, true, settings, 11);
    assert(state.resetStatus == JogconResetStatus::returning);
    assert(state.motorCommand == 0x2f && state.motorPower == 15);
    jogcon_step(state, -50, 2, true, true, settings, 12);
    assert(state.motorCommand == 0x1f);
    jogcon_step(state, 4, 0, true, true, settings, 20);
    jogcon_step(state, 4, 0, true, true, settings, 21);
    jogcon_step(state, 4, 0, true, true, settings, 119);
    assert(state.resetStatus == JogconResetStatus::returning && state.motorPower == 0);
    jogcon_step(state, 4, 0, true, true, settings, 121);
    assert(state.resetStatus == JogconResetStatus::complete);
    jogcon_step(state, -8, 0, true, true, settings, 121);
    assert(state.resetStatus == JogconResetStatus::returning && state.motorPower <= 4);
    jogcon_step(state, -2, 2, true, true, settings, 122);
    assert(state.motorPower == 0 && state.resetStatus == JogconResetStatus::returning);
    jogcon_step(state, 0, 0, true, true, settings, 123);
    jogcon_step(state, 0, 0, true, true, settings, 124);
    jogcon_step(state, 0, 0, true, true, settings, 174);
    jogcon_step(state, 0, 0, true, true, settings, 224);
    assert(state.resetStatus == JogconResetStatus::complete);
    state = JogconState{};
    state.connected = state.ready = 1;
    state.resetStatus = JogconResetStatus::complete;
    state.startTime = uint32_t(121u - 3000u);
    jogcon_step(state, 100, 0, true, true, settings, 121);
    assert(state.motorPower == 0);
    jogcon_step(state, 101, 0, true, true, settings, 122);
    assert(state.motorPower > 0 && (state.motorCommand & 0xf0) == 0x20);
    jogcon_step(state, -101, 0, true, true, settings, 123);
    assert((state.motorCommand & 0xf0) == 0x10);
    settings.forceback = 0;
    jogcon_step(state, 1000, 0, true, true, settings, 124);
    assert(state.motorCommand == 0);
    jogcon_reset(state, 125);
    jogcon_step(state, 1000, 0, true, true, settings, 126);
    assert(state.motorPower > 0); // 手動復帰はForceback無効でも動作する。
    for (unsigned t = 150; t < 3125; t += 50)
        jogcon_step(state, 1000, 0, true, true, settings, t);
    jogcon_step(state, 1000, 0, true, true, settings, 3125);
    assert(state.resetStatus == JogconResetStatus::timeout && state.motorCommand == 0);
    jogcon_reset(state, 3200);
    jogcon_step(state, INT_MIN, 0, true, true, settings, 3201);
    assert(state.motorPower <= 15);
    jogcon_step(state, 40, 4, true, true, settings, 3202);
    assert(state.resetStatus == JogconResetStatus::fault && state.motorCommand == 0);
    jogcon_reset(state, 3300);
    jogcon_step(state, 40, 0, true, true, settings, 3301);
    jogcon_guard(state, true, true, 3500);
    assert(state.motorCommand == 0);
    jogcon_guard(state, false, false, 3501);
    assert(state.motorCommand == 0);
}
