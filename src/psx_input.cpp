// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "psx_input.h"
#include "psx_latency.h"
#include "storagemanager.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <cstdio>

PsxInput& PsxInput::getInstance() {
    static PsxInput input;
    return input;
}

static uint8_t reverseBits(uint8_t value) {
    value = ((value & 0x55u) << 1) | ((value >> 1) & 0x55u);
    value = ((value & 0x33u) << 2) | ((value >> 2) & 0x33u);
    return (value << 4) | (value >> 4);
}

void PsxInput::setup() {
    if (initialized) return;
    initialized = true;
    // RXのD7はATTとして使用するため、UARTはTXだけを有効にする。
#ifndef PSX_LATENCY_TRACE
    uart_init(uart0, 115200);
    gpio_set_function(PSX_DEBUG_TX_PIN, GPIO_FUNC_UART);
    uart_puts(uart0, "\r\nGP2040 PSX: SPI0 250kHz, USB host fixed, UART0 TX\r\n");
#endif
    psx_latency_setup();
    gpio_init(PSX_ATT_PIN);
    gpio_put(PSX_ATT_PIN, 1);
    gpio_set_dir(PSX_ATT_PIN, GPIO_OUT);
    spi_init(spi0, 250000);
    // HW SPIのMode 3を使用し、LSB firstは送受信バイトのビット反転で実現する。
    spi_set_format(spi0, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(PSX_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PSX_CMD_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PSX_DATA_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(PSX_DATA_PIN);
}

void PsxInput::select(bool active) {
    gpio_put(PSX_ATT_PIN, active ? 0 : 1);
    busy_wait_us_32(50);
}

uint8_t PsxInput::exchangeByte(uint8_t value) {
    uint8_t tx = reverseBits(value), rx = 0xff;
    spi_write_read_blocking(spi0, &tx, &rx, 1);
    // ACKは未接続。動作確認済みサンプルと同じバイト間待機を使用する。
    busy_wait_us_32(50);
    return reverseBits(rx);
}

bool PsxInput::readPacket(uint8_t* response, size_t& length) {
    select(true);
    response[0] = exchangeByte(0x01);
    response[1] = exchangeByte(0x42);
    response[2] = exchangeByte(0x00);
    length = 3;
    // 対応IDだけ受信する。未接続や未対応機種の応答を入力として採用しない。
    const uint8_t id = response[1];
    const bool valid = response[2] == 0x5a &&
        (id == 0x41 || id == 0x53 || id == 0x73 || id == 0x79 || id == 0x23 || id == 0xe3);
    const bool configResponse = response[2] == 0x5a && id == 0xf3;
    if (valid || configResponse) {
        const size_t target = 3 + 2 * (id & 0x0f);
        while (length < target) {
            uint8_t tx = 0x5a;
            if (id == 0xe3) {
                tx = 0x00; // Rust方式では第2指令スロットとパディングをゼロにする。
                // 実機未検証の候補。初期化確認済みJOGCONだけモーター指令を送る。
                if (length == 3) tx = state.jogcon && state.jogReady ? jogState.motorCommand : 0;
            }
            response[length++] = exchangeByte(tx);
        }
    }
    select(false);
    // 再起動・途中抜去で設定モードに残ったパッドを復旧する。
    // F3は通常入力として採用せず、全応答を受け取ってから退出だけを送る。
    if (configResponse && time_us_64() >= nextConfigRecovery) {
        static const uint8_t exitConfig[] = {0x01, 0x43, 0x00, 0x00, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a};
        select(true);
        for (uint8_t value : exitConfig) exchangeByte(value);
        select(false);
        nextConfigRecovery = time_us_64() + 100000;
    }
    return valid;
}

void PsxInput::configureStep() {
    // 一度に一コマンドのみ。再接続でUSB処理を秒単位停止させない。
    static const uint8_t commands[][9] = {
        {0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00},
        {0x01, 0x43, 0x00, 0x00, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a}
    };
    select(true);
    for (uint8_t value : commands[configureStage - 1]) exchangeByte(value);
    select(false);
    if (++configureStage > 3) {
        configureStage = 0;
        ++configureAttempts;
    }
    // 0x41のままでもDigital→DSへの交換を検出できるよう、低頻度で再設定する。
    // 設定コマンド間の待機を1msにし、Digital PADの定期確認による40ms停止を抑える。
    nextConfigure = time_us_64() + (configureStage ? 1000 : 1000000);
}

void PsxInput::disconnect() {
    jogcon_guard(jogState, false, false, static_cast<uint32_t>(time_us_64() / 1000));
    publishJogState();
    jogResetRequested = false;
    jogConfigureFailed = false;
    configureStageJog = 0;
    if (state.connected) {
        uint32_t generation = state.generation + 1;
        uint32_t sequence = state.frameSequence;
        state = PsxSnapshot{};
        state.generation = generation;
        state.frameSequence = sequence;
        state.timestampUs = time_us_64();
#ifndef PSX_LATENCY_TRACE
        uart_puts(uart0, "PSX disconnected / unsupported: neutral\r\n");
#endif
    }
    configureStage = 0;
    configureAttempts = 0;
}

bool PsxInput::requestJogconReset() {
    if (!state.connected || !state.jogcon || !state.jogReady || state.jogSleepSuspected) return false;
    jogResetRequested = true;
    return true;
}

void PsxInput::publishJogState() {
#ifndef PSX_LATENCY_TRACE
    const uint64_t now = time_us_64();
    if (state.jogMotorCommand != jogState.motorCommand && (now >= nextJogLog || !jogState.motorCommand)) {
        nextJogLog = now + 100000;
        char message[48];
        std::snprintf(message, sizeof(message), "JOGCON motor=%02X power=%u reset=%u\r\n",
            jogState.motorCommand, jogState.motorPower, static_cast<unsigned>(jogState.resetStatus));
        uart_puts(uart0, message);
    }
#endif
    state.jogMotorCommand = jogState.motorCommand;
    state.jogMotorPower = jogState.motorPower;
    state.jogResetState = static_cast<uint8_t>(jogState.resetStatus);
    state.jogReady = jogState.ready != 0;
}

void PsxInput::updateJogGuard(uint32_t nowMs) {
    if (state.connected && state.jogcon) {
        state.jogSleepSuspected = time_us_64() - lastJogButtonUs >= 60000000;
        if (state.jogSleepSuspected) {
            jogState.motorCommand = jogState.motorPower = 0;
            publishJogState();
            return;
        }
    }
    // 設定中の500ms待機は通常パケットの欠落ではない。駆動せず完了後に復帰を開始する。
    if (configureStageJog) {
        jogState.motorCommand = jogState.motorPower = 0;
        publishJogState();
        return;
    }
    jogcon_guard(jogState, state.connected && state.jogcon, state.jogReady, nowMs);
    publishJogState();
}

void PsxInput::configureJogStep() {
    // Rust方式のpoll→設定開始→Analog→JOGCON専用割当→設定終了。
    // 応答確認と期限は維持する。DualShock用第2モーターを割り当てない。
    static const uint8_t commands[][9] = {
        {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x01, 0x44, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x01, 0x4d, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff},
        {0x01, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    };
    const uint64_t now = time_us_64();
    if (configureStageJog == 6) {
        configureStageJog = 0;
        jogState.ready = !jogConfigureFailed;
        if (jogState.ready) jogcon_reset(jogState, static_cast<uint32_t>(now / 1000));
        publishJogState();
        return;
    }
    // 試行期限は実際の初回送信時から計時する。
    if (!jogConfigureAttemptStarted) {
        jogConfigureStarted = now;
        jogConfigureAttemptStarted = true;
    }
    const uint8_t* command = commands[configureStageJog - 1];
    select(true);
    for (size_t i = 0; i < 9; ++i) jogConfigureResponse[i] = exchangeByte(command[i]);
    select(false);
    const bool valid = jogConfigureResponse[2] == 0x5a;
    if (jogConfigTraceCount < 40) {
        for (unsigned i = 0; i < 9; ++i) jogConfigReplies[jogConfigTraceCount][i] = jogConfigureResponse[i];
        jogConfigElapsed[jogConfigTraceCount] = static_cast<uint32_t>(now - jogConfigureStarted);
        auto& trace = jogConfigTrace[jogConfigTraceCount++];
        trace[0] = configureStageJog;
        trace[1] = jogConfigureResponse[1];
        trace[2] = jogConfigureResponse[2];
        trace[3] = jogConfigureResponse[3];
    }
    const bool acknowledged = valid && (configureStageJog == 1 || configureStageJog == 5
        ? jogConfigureResponse[1] == 0xe3 : jogConfigureResponse[1] == 0xf3);
    if (acknowledged) ++jogConfigureCount;
    const bool repeat = false;
    const bool completed = acknowledged && (!repeat || jogConfigureCount >= 3);
    if (completed) {
        ++configureStageJog;
        jogConfigureCount = 0;
        nextConfigure = now + 1000;
        jogConfigureAttemptStarted = false;
    } else if (now - jogConfigureStarted >= 250000) {
        jogConfigureFailed = true;
        jogState.resetStatus = JogconResetStatus::fault;
        jogState.ready = 0;
        jogState.motorCommand = jogState.motorPower = 0;
        // 失敗時も退出だけ試す。再接続まで駆動禁止。
        configureStageJog = configureStageJog < 5 ? 5 : 0;
        jogConfigureCount = 0;
        nextConfigure = now + 10000;
        jogConfigureAttemptStarted = false;
#ifndef PSX_LATENCY_TRACE
        uart_puts(uart0, "JOGCON config response unsupported: motor off\r\n");
#endif
    } else {
        nextConfigure = now + 10000;
    }
    publishJogState();
}

void PsxInput::poll() {
    const uint64_t now = time_us_64();
    updateJogGuard(static_cast<uint32_t>(now / 1000));
    if (now < nextPoll) return;
    // SPI転送時間以下には詰めない。通常約1ms周期で取得し、余分な4ms待ちをなくす。
    nextPoll = now + 1000;
    if (configureStageJog) {
        if (now >= nextConfigure) configureJogStep();
        return;
    }
    if (configureStage) {
        // 設定モード中のpollは0xF3を返すため、通常入力と混ぜて切断扱いにしない。
        if (now >= nextConfigure) configureStep();
        return;
    }
    uint8_t response[21] = {};
    size_t length = 0;
    if (!readPacket(response, length)) {
        disconnect();
        nextPoll = now + 20000;
        return;
    }
    PsxSnapshot next;
    next.id = response[1];
    next.connected = true;
    next.generation = state.generation;
    next.frameSequence = state.frameSequence + 1;
    next.timestampUs = time_us_64();
    next.buttons = static_cast<uint16_t>(~(response[3] | (response[4] << 8)));
    next.negcon = next.id == 0x23;
    next.jogcon = next.id == 0xe3;
    next.analog = next.id != 0x41 && !next.jogcon;
    const bool machineChanged = !state.connected || state.id != next.id;
    if (next.jogcon) {
        const uint32_t raw = uint32_t(response[5]) | (uint32_t(response[6]) << 8);
        next.jogPosition = raw & 0x8000u ? static_cast<int32_t>(raw) - 0x10000 : static_cast<int32_t>(raw);
        next.jogStatus = response[7];
        next.jogReserved = response[8];
    } else if (next.negcon) {
        next.lx = response[5];
        next.pressureI = response[6];
        next.pressureII = response[7];
        next.pressureL = response[8];
        // neGconの未実装デジタルビットは無視し、押込の閾値判定は仮想入力で行う。
        next.buttons &= (1u << 3) | (0xfu << 4) | (1u << 11) | (1u << 12) | (1u << 13);
    } else if (next.analog) {
        next.rx = response[5]; next.ry = response[6];
        next.lx = response[7]; next.ly = response[8];
    }
    if (machineChanged) {
        ++next.generation;
        jogState = JogconState{};
        jogResetRequested = false;
        jogConfigureFailed = false;
        configureStageJog = 0;
        if (next.jogcon) {
            configureStage = 0;
            configureStageJog = 1;
            jogConfigTraceCount = 0;
            jogConfigureCount = 0;
            jogConfigureStarted = now;
            lastJogButtonUs = now;
            jogConfigureAttemptStarted = false;
            nextConfigure = now;
        }
#ifndef PSX_LATENCY_TRACE
        uart_puts(uart0, next.jogcon ? "PSX connected: JOGCON RAW\r\n" : next.negcon ? "PSX connected: neGcon\r\n" :
            next.analog ? "PSX connected: analog\r\n" : "PSX connected: digital\r\n");
#endif
    }
    if (state.connected && (state.buttons != next.buttons || state.lx != next.lx || state.ly != next.ly
        || state.rx != next.rx || state.ry != next.ry || state.pressureI != next.pressureI
        || state.pressureII != next.pressureII || state.pressureL != next.pressureL
        || state.jogPosition != next.jogPosition || state.jogStatus != next.jogStatus))
        psx_latency_received();
    const uint8_t previousMotorCommand = state.jogMotorCommand;
    // HTTP取得間に短い解放・押下があっても、新しい押下イベントを識別する。
    next.buttonEventSequence = state.buttonEventSequence;
    next.buttonEventMask = state.buttonEventMask;
    const uint16_t pressed = next.buttons & ~state.buttons;
    state = next;
    if (pressed) {
        ++state.buttonEventSequence;
        state.buttonEventMask = virtualButtons();
    }
    state.jogMotorCommand = previousMotorCommand;
    if (state.jogcon) {
        if (state.buttons) lastJogButtonUs = next.timestampUs;
        state.jogSleepSuspected = next.timestampUs - lastJogButtonUs >= 60000000;
        const AnalogOptions& options = Storage::getInstance().getAddonOptions().analogOptions;
        const JogconSettings settings{options.has_jogconDeadzone ? options.jogconDeadzone : 0,
            options.has_jogconMax ? options.jogconMax : 100,
            static_cast<uint8_t>(options.has_jogconForcebackEnabled ? options.jogconForcebackEnabled : true)};
        if (jogResetRequested) {
            jogcon_reset(jogState, static_cast<uint32_t>(next.timestampUs / 1000));
            jogResetRequested = false;
        }
        jogcon_step(jogState, state.jogPosition, state.jogStatus, true, jogState.ready != 0,
            settings, static_cast<uint32_t>(next.timestampUs / 1000));
        if (state.jogSleepSuspected) jogState.motorCommand = jogState.motorPower = 0;
    }
    publishJogState();
    // Digital PADは設定コマンドを無視する。DSはデジタル起動からアナログへ切替。
    if (state.id == 0x41 && !configureStage && now >= nextConfigure) {
        configureStage = 1;
        nextConfigure = now;
    }
}

uint32_t PsxInput::virtualButtons() const {
    if (!state.connected) return 0;
    uint16_t buttons = state.buttons;
    if (state.negcon) {
        const AnalogOptions& options = Storage::getInstance().getAddonOptions().analogOptions;
        if ((!options.has_negconIDigitalEnabled || options.negconIDigitalEnabled)
            && state.pressureI && state.pressureI >= options.negconIThreshold) buttons |= 1u << 14;
        if ((!options.has_negconIIDigitalEnabled || options.negconIIDigitalEnabled)
            && state.pressureII && state.pressureII >= options.negconIIThreshold) buttons |= 1u << 15;
        if ((!options.has_negconLDigitalEnabled || options.negconLDigitalEnabled)
            && state.pressureL && state.pressureL >= options.negconLThreshold) buttons |= 1u << 10;
    }
    // 保存形式は既存の30スロットを利用するが、実GPIOとは完全に分離する。
    static constexpr uint8_t slots[16] = {20, 22, 23, 21, 8, 10, 9, 11, 15, 14, 19, 18, 17, 13, 12, 16};
    uint32_t result = 0;
    for (unsigned bit = 0; bit < 16; ++bit) {
        if (buttons & (1u << bit)) result |= 1u << slots[bit];
    }
    return result;
}
