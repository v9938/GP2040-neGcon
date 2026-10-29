#include "addons/input_macro.h"
#include "storagemanager.h"
#include "GamepadState.h"

bool InputMacro::available() {
    return Storage::getInstance().getAddonOptions().macroOptions.enabled;
}

void InputMacro::setup() {
    inputMacroOptions = &Storage::getInstance().getAddonOptions().macroOptions;
    reinit();
}

void InputMacro::reinit() {
    macroButtonMask = 0;
    memset(macroPinMasks, 0, sizeof(macroPinMasks));
    const auto* mappings = Storage::getInstance().getProfilePinMappings();
    for (Pin_t pin = 0; pin < NUM_BANK0_GPIOS; ++pin) {
        const auto action = mappings[pin].action;
        if (action == BUTTON_PRESS_MACRO) {
            macroButtonMask |= 1u << pin;
        } else if (action >= BUTTON_PRESS_MACRO_1 && action <= BUTTON_PRESS_MACRO_6) {
            macroPinMasks[action - BUTTON_PRESS_MACRO_1] |= 1u << pin;
        }
    }
    prevMacroInputPressed = false;
    reset();
}

void InputMacro::reset() {
    macroPosition = -1;
    pressedMacro = -1;
    isMacroRunning = false;
    isMacroTriggerHeld = false;
    macroStartTime = 0;
    macroInputPosition = 0;
    macroInputHoldTime = INPUT_HOLD_US;
}

void InputMacro::restart(Macro& macro) {
    macroStartTime = currentMicros;
    macroInputPosition = 0;
    const auto& input = macro.macroInputs[0];
    macroInputHoldTime = uint64_t(input.duration) + input.waitDuration;
    if (!macroInputHoldTime) macroInputHoldTime = INPUT_HOLD_US;
}

void InputMacro::checkMacroPress() {
    const Gamepad* gamepad = Storage::getInstance().GetGamepad();
    const Mask_t allPins = gamepad->debouncedGpio;
    pressedMacro = -1;
    for (int i = 0; i < MAX_MACRO_LIMIT; ++i) {
        const auto& macro = inputMacroOptions->macroList[i];
        if (!macro.enabled || !macro.macroInputs_count || macro.macroInputs_count > MAX_MACRO_INPUT_LIMIT) continue;
        const bool triggered = macro.useMacroTriggerButton
            ? (allPins & macroButtonMask) &&
                ((gamepad->state.buttons & (macro.macroTriggerButton & 0xfff0ffffu)) ||
                 (gamepad->state.dpad & ((macro.macroTriggerButton >> 16) & GAMEPAD_MASK_DPAD)))
            : (allPins & macroPinMasks[i]);
        if (triggered) {
            pressedMacro = i;
            break;
        }
    }
}

void InputMacro::checkMacroAction() {
    const bool pressed = pressedMacro >= 0;
    const bool newPress = pressed && !prevMacroInputPressed;
    prevMacroInputPressed = pressed;
    if (!isMacroRunning) macroPosition = pressedMacro;
    if (macroPosition < 0 || macroPosition >= MAX_MACRO_LIMIT) return;
    auto& macro = inputMacroOptions->macroList[macroPosition];
    if (!macro.enabled || !macro.macroInputs_count || macro.macroInputs_count > MAX_MACRO_INPUT_LIMIT) {
        reset();
        return;
    }
    if (macro.macroType == ON_HOLD_REPEAT) {
        isMacroTriggerHeld = pressedMacro == macroPosition;
    } else if (macro.macroType == ON_TOGGLE && isMacroRunning && newPress) {
        reset();
        return;
    } else if (!isMacroRunning) {
        isMacroTriggerHeld = newPress;
    }
    if (!isMacroRunning && isMacroTriggerHeld) {
        restart(macro);
        isMacroRunning = true;
    }
}

void InputMacro::runCurrentMacro() {
    if (!isMacroRunning || macroPosition < 0 || macroPosition >= MAX_MACRO_LIMIT) return;
    auto& macro = inputMacroOptions->macroList[macroPosition];
    if (!macro.enabled || !macro.macroInputs_count || macro.macroInputs_count > MAX_MACRO_INPUT_LIMIT ||
        macroInputPosition >= int(macro.macroInputs_count) ||
        (macro.macroType == ON_HOLD_REPEAT && !isMacroTriggerHeld)) {
        reset();
        return;
    }
    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    if (!macro.interruptible && macro.exclusive) {
        // Exclusivity is digital only; the PSX analog axes remain untouched.
        gamepad->state.dpad = 0;
        gamepad->state.buttons = 0;
    } else {
        if (macro.useMacroTriggerButton) {
            gamepad->state.dpad &= ~((macro.macroTriggerButton >> 16) & GAMEPAD_MASK_DPAD);
            gamepad->state.buttons &= ~(macro.macroTriggerButton & 0xfff0ffffu);
        }
        if (macro.interruptible && (gamepad->state.buttons || gamepad->state.dpad)) {
            reset();
            return;
        }
    }
    if (currentMicros - macroStartTime >= macroInputHoldTime) {
        macroStartTime = currentMicros;
        if (++macroInputPosition >= int(macro.macroInputs_count)) {
            if (macro.macroType == ON_PRESS) {
                reset();
                return;
            }
            restart(macro);
        } else {
            const auto& input = macro.macroInputs[macroInputPosition];
            macroInputHoldTime = uint64_t(input.duration) + input.waitDuration;
            if (!macroInputHoldTime) macroInputHoldTime = INPUT_HOLD_US;
        }
    }
    const auto& input = macro.macroInputs[macroInputPosition];
    if (currentMicros - macroStartTime < input.duration) {
        gamepad->state.dpad |= (input.buttonMask >> 16) & GAMEPAD_MASK_DPAD;
        gamepad->state.buttons |= input.buttonMask & 0xfff0ffffu;
    }
}

void InputMacro::preprocess() {
    if (!inputMacroOptions->enabled) {
        reset();
        prevMacroInputPressed = false;
        return;
    }
    currentMicros = time_us_64();
    checkMacroPress();
    checkMacroAction();
    runCurrentMacro();
}
