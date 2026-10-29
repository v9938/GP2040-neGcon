#include "addons/reverse.h"
#include "storagemanager.h"
#include "GamepadEnums.h"
#include "helper.h"
#include "config.pb.h"

bool ReverseInput::available() {
    const ReverseOptions& options = Storage::getInstance().getAddonOptions().reverseOptions;
	return options.enabled;
}

void ReverseInput::setup()
{
    // Setup Reverse Input Button
    mapInputReverse = new GamepadButtonMapping(0);

    GpioMappingInfo* pinMappings = Storage::getInstance().getProfilePinMappings();
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        switch (pinMappings[pin].action) {
            case GpioAction::BUTTON_PRESS_INPUT_REVERSE: mapInputReverse->pinMask |= 1u << pin; break;
            default:    break;
        }
    }

    const ReverseOptions& options = Storage::getInstance().getAddonOptions().reverseOptions;

    actionUp = options.actionUp <= 2 ? options.actionUp : 0;
    actionDown = options.actionDown <= 2 ? options.actionDown : 0;
    actionLeft = options.actionLeft <= 2 ? options.actionLeft : 0;
    actionRight = options.actionRight <= 2 ? options.actionRight : 0;

    Gamepad * gamepad = Storage::getInstance().GetGamepad();
    mapDpadUp    = gamepad->mapDpadUp;
    mapDpadDown  = gamepad->mapDpadDown;
    mapDpadLeft  = gamepad->mapDpadLeft;
    mapDpadRight = gamepad->mapDpadRight;

    state = false;
}

void ReverseInput::update() {
    Mask_t values = Storage::getInstance().GetGamepad()->debouncedGpio;

    state = (values & mapInputReverse->pinMask);
}
void ReverseInput::reinit() {
    delete mapInputReverse;
    setup();
}

uint8_t ReverseInput::input(uint32_t valueMask, uint16_t buttonMask, uint16_t buttonMaskReverse, uint8_t action) {
    if (state && action == 2) {
        return 0;
    }
    // Base axis inversion is applied once later by Gamepad::process().
    bool invert = state && action == 1;
    return (valueMask ? (invert ? buttonMaskReverse : buttonMask) : 0);
}

void ReverseInput::preprocess()
{
    if (!Storage::getInstance().getAddonOptions().reverseOptions.enabled) return;
    // Update Reverse State
    update();

    Gamepad * gamepad = Storage::getInstance().GetGamepad();

    gamepad->state.dpad = 0
        | input(gamepad->state.dpad & mapDpadUp->buttonMask,    mapDpadUp->buttonMask,      mapDpadDown->buttonMask,    actionUp)
        | input(gamepad->state.dpad & mapDpadDown->buttonMask,  mapDpadDown->buttonMask,    mapDpadUp->buttonMask,      actionDown)
        | input(gamepad->state.dpad & mapDpadLeft->buttonMask,  mapDpadLeft->buttonMask,    mapDpadRight->buttonMask,   actionLeft)
        | input(gamepad->state.dpad & mapDpadRight->buttonMask, mapDpadRight->buttonMask,   mapDpadLeft->buttonMask,    actionRight)
    ;
}
