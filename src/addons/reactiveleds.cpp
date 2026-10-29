#include "addons/reactiveleds.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "helper.h"
#include "xiao_leds.h"

#include <algorithm>

namespace {
uint8_t directionLevel(uint16_t value, uint16_t mid, bool positive) {
    // 中心の左右で幅が異なるため、16bit全域を各方向の0..255に正規化する。
    const int32_t distance = positive ? int32_t(value) - mid : int32_t(mid) - value;
    const uint32_t width = positive ? GAMEPAD_JOYSTICK_MAX - mid : mid;
    return distance > 0 && width ? uint32_t(distance) * 255 / width : 0;
}

uint8_t actionLevel(Gamepad* gamepad, GpioAction action, uint16_t mid) {
    switch (action) {
        case BUTTON_PRESS_UP: return gamepad->pressedDpad(GAMEPAD_MASK_UP) ? 255 : 0;
        case BUTTON_PRESS_DOWN: return gamepad->pressedDpad(GAMEPAD_MASK_DOWN) ? 255 : 0;
        case BUTTON_PRESS_LEFT: return gamepad->pressedDpad(GAMEPAD_MASK_LEFT) ? 255 : 0;
        case BUTTON_PRESS_RIGHT: return gamepad->pressedDpad(GAMEPAD_MASK_RIGHT) ? 255 : 0;
        case BUTTON_PRESS_B1: return gamepad->pressedButton(GAMEPAD_MASK_B1) ? 255 : 0;
        case BUTTON_PRESS_B2: return gamepad->pressedButton(GAMEPAD_MASK_B2) ? 255 : 0;
        case BUTTON_PRESS_B3: return gamepad->pressedButton(GAMEPAD_MASK_B3) ? 255 : 0;
        case BUTTON_PRESS_B4: return gamepad->pressedButton(GAMEPAD_MASK_B4) ? 255 : 0;
        case BUTTON_PRESS_L1: return gamepad->pressedButton(GAMEPAD_MASK_L1) ? 255 : 0;
        case BUTTON_PRESS_R1: return gamepad->pressedButton(GAMEPAD_MASK_R1) ? 255 : 0;
        case BUTTON_PRESS_L2: return gamepad->pressedButton(GAMEPAD_MASK_L2) ? 255 : 0;
        case BUTTON_PRESS_R2: return gamepad->pressedButton(GAMEPAD_MASK_R2) ? 255 : 0;
        case BUTTON_PRESS_S1: return gamepad->pressedButton(GAMEPAD_MASK_S1) ? 255 : 0;
        case BUTTON_PRESS_S2: return gamepad->pressedButton(GAMEPAD_MASK_S2) ? 255 : 0;
        case BUTTON_PRESS_A1: return gamepad->pressedButton(GAMEPAD_MASK_A1) ? 255 : 0;
        case BUTTON_PRESS_A2: return gamepad->pressedButton(GAMEPAD_MASK_A2) ? 255 : 0;
        case BUTTON_PRESS_L3: return gamepad->pressedButton(GAMEPAD_MASK_L3) ? 255 : 0;
        case BUTTON_PRESS_R3: return gamepad->pressedButton(GAMEPAD_MASK_R3) ? 255 : 0;
        case ANALOG_DIRECTION_LS_X_NEG: return directionLevel(gamepad->state.lx, mid, false);
        case ANALOG_DIRECTION_LS_X_POS: return directionLevel(gamepad->state.lx, mid, true);
        case ANALOG_DIRECTION_LS_Y_NEG: return directionLevel(gamepad->state.ly, mid, false);
        case ANALOG_DIRECTION_LS_Y_POS: return directionLevel(gamepad->state.ly, mid, true);
        case ANALOG_DIRECTION_RS_X_NEG: return directionLevel(gamepad->state.rx, mid, false);
        case ANALOG_DIRECTION_RS_X_POS: return directionLevel(gamepad->state.rx, mid, true);
        case ANALOG_DIRECTION_RS_Y_NEG: return directionLevel(gamepad->state.ry, mid, false);
        case ANALOG_DIRECTION_RS_Y_POS: return directionLevel(gamepad->state.ry, mid, true);
        default: return 0;
    }
}
}

bool ReactiveLEDAddon::available() {
    // 無効時も固定電源ピンをLowにするためロードする。旧pin設定は使わない。
    return true;
}

void ReactiveLEDAddon::setup() {
    const ReactiveLEDOptions& options = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    enabled = options.enabled;
    lastProcess = getMillis();
    for (uint8_t led = 0; led < REACTIVE_LED_COUNT; led++) {
        ledPins[led] = ReactiveLEDPinState{};
        if (led < options.leds_count) {
            ledPins[led].action = options.leds[led].action;
            ledPins[led].modeDown = options.leds[led].modeDown;
            ledPins[led].modeUp = options.leds[led].modeUp;
            ledPins[led].brightness = std::min<uint32_t>(options.leds[led].brightness, 255);
        }
        ledPins[led].lastUpdate = lastProcess;
        ledPins[led].currUpdate = lastProcess;
        setLEDByMode(ledPins[led], 0);
    }
    xiaoNeoPixelInitialize(enabled);
}

void ReactiveLEDAddon::process() {
    const uint32_t now = getMillis();
    if (enabled && now - lastProcess >= 1) {
        lastProcess = now;
        Gamepad* gamepad = Storage::getInstance().GetProcessedGamepad();
        GPDriver* driver = DriverManager::getInstance().getDriver();
        const uint16_t mid = driver ? driver->GetJoystickMidValue() : GAMEPAD_JOYSTICK_MID;
        uint8_t rgb[REACTIVE_LED_COUNT] = {};
        for (uint8_t led = 0; led < REACTIVE_LED_COUNT; led++) {
            ReactiveLEDPinState& state = ledPins[led];
            state.currUpdate = now;
            setLEDByMode(state, gamepad ? actionLevel(gamepad, state.action, mid) : 0);
            rgb[led] = uint16_t(state.value) * state.brightness / 255;
        }
        xiaoNeoPixelSet(rgb[0], rgb[1], rgb[2]);
    }
    xiaoNeoPixelProcess();
}

void ReactiveLEDAddon::setLEDByMode(ReactiveLEDPinState& state, uint8_t level) {
    const bool pressed = level > 0;
    const ReactiveLEDMode mode = pressed ? state.modeDown : state.modeUp;
    const bool edge = !state.initialized || state.prevState != pressed;
    state.currState = pressed;
    if (state.action == NONE) {
        state.value = 0;
    } else {
        // エッジでの初期値はtick判定と独立させ、短い入力でも必ず反映する。
        if (edge) {
            if (mode == REACTIVE_LED_FADE_IN) state.value = 0;
            if (mode == REACTIVE_LED_FADE_OUT) state.value = 255;
            state.lastUpdate = state.currUpdate;
        }
        switch (mode) {
            case REACTIVE_LED_STATIC_OFF: state.value = 0; break;
            case REACTIVE_LED_STATIC_ON: state.value = 255; break;
            case REACTIVE_LED_ANALOG: state.value = level; break;
            case REACTIVE_LED_FADE_IN:
            case REACTIVE_LED_FADE_OUT:
                if (state.currUpdate - state.lastUpdate >= REACTIVE_LED_DELAY) {
                    state.value = mode == REACTIVE_LED_FADE_IN
                        ? std::min<int>(255, state.value + REACTIVE_LED_FADE_INC)
                        : std::max<int>(0, state.value - REACTIVE_LED_FADE_INC);
                    state.lastUpdate = state.currUpdate;
                }
                break;
            default: state.value = 0; break;
        }
    }
    state.prevState = pressed;
    state.initialized = true;
}
