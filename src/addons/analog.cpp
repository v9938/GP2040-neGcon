#include "addons/analog.h"
#include "analog_preview.h"
#include "config.pb.h"
#include "drivermanager.h"
#include "peripheralmanager.h"
#include "psx_input.h"
#include "storagemanager.h"

#include <algorithm>
#include <cmath>

bool AnalogInput::available() {
    return true;
}

void AnalogInput::setup() {
    const AnalogOptions& options = Storage::getInstance().getAddonOptions().analogOptions;
    sticks[0].analog_invert = options.analogAdc1Invert;
    sticks[0].analog_dpad = options.has_analogAdc1Mode ? options.analogAdc1Mode : DPAD_MODE_LEFT_ANALOG;
    sticks[0].ema_option = options.analog_smoothing;
    sticks[0].ema_smoothing = options.smoothing_factor;
    sticks[0].error_rate = options.analog_error;
    sticks[0].in_deadzone = options.inner_deadzone;
    sticks[0].out_deadzone = options.outer_deadzone;
    sticks[0].auto_calibration = options.auto_calibrate;
    sticks[0].forced_circularity = options.forced_circularity;
    sticks[1].analog_invert = options.analogAdc2Invert;
    sticks[1].analog_dpad = options.has_analogAdc2Mode ? options.analogAdc2Mode : DPAD_MODE_RIGHT_ANALOG;
    sticks[1].ema_option = options.analog_smoothing2;
    sticks[1].ema_smoothing = options.smoothing_factor2;
    sticks[1].error_rate = options.analog_error2;
    sticks[1].in_deadzone = options.inner_deadzone2;
    sticks[1].out_deadzone = options.outer_deadzone2;
    sticks[1].auto_calibration = options.auto_calibrate2;
    sticks[1].forced_circularity = options.forced_circularity2;

    for (auto &stick : sticks) {
        // Keep storage units here; the shared transform performs the original divisions.
        stick.ema_smoothing = std::isfinite(stick.ema_smoothing)
            ? std::clamp(stick.ema_smoothing, 0.0f, 1000.0f) : 0.0f;
        stick.in_deadzone = std::clamp(stick.in_deadzone, 0.0f, 100.0f);
        stick.out_deadzone = std::clamp(stick.out_deadzone, 0.0f, 100.0f);
    }
    // 旧保存データに追加フィールドがない場合もneGconの既定割り当てを使う。
    pressure_modes[0] = options.has_negconIMode ? options.negconIMode : 12;
    pressure_modes[1] = options.has_negconIIMode ? options.negconIIMode : 11;
    pressure_modes[2] = options.has_negconLMode ? options.negconLMode : 0;
    resetCalibration();
    last_generation = 0;
    last_id = 0;
    last_connected = last_analog = last_negcon = false;
    // reinit後も旧出力の所有情報を残し、次のprocessで解放する。
}

void AnalogInput::resetCalibration() {
    for (auto &stick : sticks) {
        stick.x_center = stick.y_center = 128;
        stick.x_ema = stick.y_ema = 0.5f;
        stick.calibrated = false;
    }
}

void AnalogInput::processStick(int stick_num, uint8_t raw_x, uint8_t raw_y,
    bool one_axis, float &x, float &y) {
    auto &stick = sticks[stick_num];
    // One transform per original process call; do not gate EMA on frameSequence.
    const AnalogPreviewStickSettings settings = {
        static_cast<uint32_t>(stick.analog_dpad), static_cast<uint32_t>(stick.analog_invert),
        static_cast<uint32_t>(stick.in_deadzone), static_cast<uint32_t>(stick.out_deadzone),
        stick.ema_smoothing, static_cast<uint32_t>(stick.error_rate),
        stick.ema_option, stick.forced_circularity, stick.auto_calibration, 0};
    AnalogPreviewStickState state = {stick.x_ema, stick.y_ema,
        stick.x_center, stick.y_center, stick.calibrated, 0};
    analog_transform_stick(raw_x, raw_y, one_axis, &settings, &state, &x, &y);
    stick.x_ema = state.xEma;
    stick.y_ema = state.yEma;
    stick.x_center = state.xCenter;
    stick.y_center = state.yCenter;
    stick.calibrated = state.calibrated;
}

void AnalogInput::preprocess() {
    // USB前処理には全チャネルの基準値を渡し、PSX処理後だけ所有マスクで保護する。
    Storage::getInstance().GetGamepad()->psxAnalogOutputs = 0;
}

void AnalogInput::process() {
    Gamepad *gamepad = Storage::getInstance().GetGamepad();
    const PsxSnapshot& snapshot = PsxInput::getInstance().snapshot();
    uint32_t midpoint = GAMEPAD_JOYSTICK_MID;
    if (DriverManager::getInstance().getDriver() != nullptr)
        midpoint = DriverManager::getInstance().getDriver()->GetJoystickMidValue();
    const auto convert = [midpoint](float value) { return analog_preview_convert(value, midpoint); };
    uint16_t *axes[] = {&gamepad->state.lx, &gamepad->state.ly,
        &gamepad->state.rx, &gamepad->state.ry};
    uint8_t *triggers[] = {&gamepad->state.lt, &gamepad->state.rt};
    const auto pairOffset = [](DpadMode mode) {
        return mode == DPAD_MODE_LEFT_ANALOG ? 0 : mode == DPAD_MODE_RIGHT_ANALOG ? 2 : -1;
    };
    uint8_t outputs = 0;
    if (snapshot.connected && (snapshot.jogcon || snapshot.analog || snapshot.negcon)) {
        for (int i = 0; i < (snapshot.jogcon || snapshot.negcon ? 1 : ADC_COUNT); ++i) {
            const int offset = pairOffset(sticks[i].analog_dpad);
            if (offset >= 0)
                outputs |= (snapshot.jogcon || snapshot.negcon ? 1u : 3u) << offset;
        }
        if (!snapshot.jogcon && snapshot.negcon) {
            // デジタルL2/R2も既存トリガーとして保護する。圧力割当なしでも有効。
            outputs |= 0x30;
            for (uint32_t mode : pressure_modes) {
                if (mode >= 1 && mode <= 12)
                    outputs |= 1u << ((mode - 1) / 3);
                else if (mode == 13 || mode == 14)
                    outputs |= 1u << (mode - 9);
            }
        }
    }
    gamepad->psxAnalogOutputs = outputs;
    const bool changed = snapshot.generation != last_generation || snapshot.id != last_id
        || snapshot.connected != last_connected || snapshot.analog != last_analog
        || snapshot.negcon != last_negcon;
    last_generation = snapshot.generation;
    last_id = snapshot.id;
    last_connected = snapshot.connected;
    last_analog = snapshot.analog;
    last_negcon = snapshot.negcon;

    // USBHostはpreprocessで先に書き込む。PSX不在時はその状態/能力を消さない。
    const bool usb_host = Storage::getInstance().getAddonOptions().gamepadUSBHostOptions.enabled
        && PeripheralManager::getInstance().isUSBEnabled(0);
    const bool preserve_host_capabilities = usb_host && !snapshot.jogcon;
    gamepad->hasLeftAnalogStick = preserve_host_capabilities || (outputs & 0x03);
    gamepad->hasRightAnalogStick = preserve_host_capabilities || (outputs & 0x0C);
    gamepad->hasAnalogTriggers = preserve_host_capabilities || (outputs & 0x30);

    const uint8_t released = usb_host ? 0 : owned_outputs & ~outputs;
    const uint8_t neutral = released | (changed ? outputs : 0);
    for (int i = 0; i < 4; ++i) {
        if (neutral & (1u << i))
            *axes[i] = midpoint;
    }
    for (int i = 0; i < 2; ++i) {
        if (neutral & (1u << (i + 4)))
            *triggers[i] = 0;
    }
    owned_outputs = outputs;
    if (changed || !snapshot.connected || (!snapshot.jogcon && !snapshot.analog && !snapshot.negcon)) {
        // 切断/機種変更のフレームは即座にneutral。中心採取は次の有効フレーム。
        resetCalibration();
        return;
    }

    if (snapshot.jogcon) {
        // Preserve dial resolution; JOGCON never passes through the 8-bit stick/pressure path.
        const int offset = pairOffset(sticks[0].analog_dpad);
        const auto& options = Storage::getInstance().getAddonOptions().analogOptions;
        if (offset >= 0) *axes[offset] = analog_preview_jogcon(snapshot.jogPosition,
            options.jogconDeadzone, options.jogconMax, sticks[0].analog_invert, midpoint);
        return;
    }

    for (int i = 0; i < (snapshot.negcon ? 1 : ADC_COUNT); ++i) {
        const int offset = pairOffset(sticks[i].analog_dpad);
        if (offset < 0)
            continue;
        float x, y;
        processStick(i, i == 0 ? snapshot.lx : snapshot.rx,
            i == 0 ? snapshot.ly : snapshot.ry, snapshot.negcon, x, y);
        *axes[offset] = convert(x);
        // neGconに存在しないYや第二スティックはUSBHost等の入力を上書きしない。
        if (!snapshot.negcon)
            *axes[offset + 1] = convert(y);
    }
    if (snapshot.negcon) {
        const auto& options = Storage::getInstance().getAddonOptions().analogOptions;
        const uint8_t pressures[] = {snapshot.pressureI, snapshot.pressureII, snapshot.pressureL};
        const uint32_t inner[] = {options.has_negconIInnerDeadzone ? options.negconIInnerDeadzone : 0,
            options.has_negconIIInnerDeadzone ? options.negconIIInnerDeadzone : 0,
            options.has_negconLInnerDeadzone ? options.negconLInnerDeadzone : 0};
        const uint32_t outer[] = {options.has_negconIOuterDeadzone ? options.negconIOuterDeadzone : 100,
            options.has_negconIIOuterDeadzone ? options.negconIIOuterDeadzone : 100,
            options.has_negconLOuterDeadzone ? options.negconLOuterDeadzone : 100};
        uint16_t values[] = {*axes[0], *axes[1], *axes[2], *axes[3]};
        uint8_t baseTriggers[] = {static_cast<uint8_t>((gamepad->state.buttons & GAMEPAD_MASK_L2) ? 255 : *triggers[0]),
            static_cast<uint8_t>((gamepad->state.buttons & GAMEPAD_MASK_R2) ? 255 : *triggers[1])};
        analog_preview_add_pressures(pressures, pressure_modes, inner, outer, midpoint, values, baseTriggers);
        for (unsigned i = 0; i < 4; ++i) *axes[i] = values[i];
        for (unsigned i = 0; i < 2; ++i) *triggers[i] = baseTriggers[i];
    }
}
