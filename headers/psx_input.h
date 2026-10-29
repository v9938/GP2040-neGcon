// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#ifndef PSX_INPUT_H
#define PSX_INPUT_H

#include <cstddef>
#include <cstdint>
#include "jogcon.h"

// XIAOのD番号とRP2040のGPIO番号は異なる。配線は設定から変更しない。
constexpr uint8_t PSX_DATA_PIN = 4; // D9
constexpr uint8_t PSX_CMD_PIN = 3;  // D10
constexpr uint8_t PSX_CLK_PIN = 2;  // D8
constexpr uint8_t PSX_ATT_PIN = 1;  // D7
constexpr uint8_t PSX_DEBUG_TX_PIN = 0; // D6 / UART0 TXのみ

struct PsxSnapshot {
    bool connected = false;
    uint8_t id = 0;
    uint16_t buttons = 0;
    uint8_t lx = 128, ly = 128, rx = 128, ry = 128;
    uint8_t pressureI = 0, pressureII = 0, pressureL = 0;
    bool analog = false;
    bool negcon = false;
    bool jogcon = false;
    int32_t jogPosition = 0;
    uint8_t jogStatus = 0, jogReserved = 0;
    uint8_t jogMotorCommand = 0, jogMotorPower = 0;
    uint8_t jogResetState = static_cast<uint8_t>(JogconResetStatus::disconnected);
    bool jogReady = false;
    bool jogSleepSuspected = false;
    uint32_t buttonEventSequence = 0;
    uint32_t buttonEventMask = 0;
    uint32_t generation = 0;
    uint32_t frameSequence = 0;
    uint64_t timestampUs = 0;
};

class PsxInput {
public:
    static PsxInput& getInstance();
    void setup();
    void poll();
    const PsxSnapshot& snapshot() const { return state; }
    uint32_t virtualButtons() const;
    bool startPressed() const { return state.connected && (state.buttons & (1u << 3)); }
    bool requestJogconReset();
private:
    uint8_t exchangeByte(uint8_t value);
    void select(bool active);
    bool readPacket(uint8_t* response, size_t& length);
    void configureStep();
    void configureJogStep();
    void updateJogGuard(uint32_t nowMs);
    void publishJogState();
    void disconnect();
    PsxSnapshot state;
    uint64_t nextPoll = 0;
    uint64_t nextConfigRecovery = 0;
    uint64_t nextConfigure = 0;
    uint8_t configureStage = 0;
    uint8_t configureAttempts = 0;
    uint8_t configureStageJog = 0;
    uint8_t jogConfigureCount = 0;
    uint64_t jogConfigureStarted = 0;
    bool jogConfigureAttemptStarted = false;
    uint8_t jogConfigureResponse[9] = {};
    uint8_t jogConfigTrace[40][4] = {};
    uint8_t jogConfigTraceCount = 0;
    uint32_t jogConfigElapsed[40] = {};
    uint8_t jogConfigReplies[40][9] = {};
    JogconState jogState;
    bool jogResetRequested = false;
    bool jogConfigureFailed = false;
    uint64_t lastJogButtonUs = 0;
    uint64_t nextJogLog = 0;
    bool initialized = false;
};

#endif
