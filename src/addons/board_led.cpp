#include "addons/board_led.h"
#include "drivermanager.h"
#include "drivers/ps4/PS4Driver.h"
#include "usbdriver.h"
#include "helper.h"
#include "xiao_leds.h"

bool BoardLedAddon::available() {
    // OFF/無効でもロードし、main入口で点灯した赤を必ず消す。
    return true;
}

void BoardLedAddon::setup() {
    const OnBoardLedOptions& options = Storage::getInstance().getAddonOptions().onBoardLedOptions;
    onBoardLedMode = options.enabled ? options.mode : ON_BOARD_LED_MODE_OFF;
    isConfigMode = DriverManager::getInstance().isConfigMode();
    timeSinceBlink = getMillis();
    prevState = -1;
    indicatorState = -1;
    blinkOn = true;
    process();
}

void BoardLedAddon::process() {
    int color = 0; // bit0=赤、bit1=緑、bit2=青
    Gamepad* gamepad = Storage::getInstance().GetProcessedGamepad();
    GPDriver* driver = DriverManager::getInstance().getDriver();
    const uint16_t mid = driver ? driver->GetJoystickMidValue() : GAMEPAD_JOYSTICK_MID;
    switch (onBoardLedMode) {
        case ON_BOARD_LED_MODE_INPUT_TEST:
            if (gamepad && (gamepad->state.buttons || gamepad->state.dpad || gamepad->state.aux ||
                gamepad->state.lt || gamepad->state.rt || gamepad->state.lx != mid ||
                gamepad->state.ly != mid || gamepad->state.rx != mid || gamepad->state.ry != mid)) {
                color = 2;
            }
            break;
        case ON_BOARD_LED_MODE_MODE_INDICATOR: {
            const int indicator = !get_usb_mounted() ? 1 : isConfigMode ? 4 : 2;
            const uint32_t now = getMillis();
            if (indicator != indicatorState) {
                indicatorState = indicator;
                timeSinceBlink = now;
                blinkOn = true;
            }
            const uint32_t interval = indicator == 1 ? BLINK_INTERVAL_USB_UNMOUNTED : BLINK_INTERVAL_CONFIG_MODE;
            if (indicator != 2 && now - timeSinceBlink >= interval) {
                timeSinceBlink = now;
                blinkOn = !blinkOn;
            }
            color = indicator == 2 || blinkOn ? indicator : 0;
            break;
        }
        case ON_BOARD_LED_MODE_PS_AUTH:
            if (!isConfigMode && driver && gamepad && (gamepad->getOptions().inputMode == INPUT_MODE_PS4 ||
                gamepad->getOptions().inputMode == INPUT_MODE_PS5) &&
                static_cast<PS4Driver*>(driver)->getAuthSent()) {
                color = 2;
            }
            break;
        case ON_BOARD_LED_MODE_OFF:
        default:
            break;
    }
    if (prevState != color) {
        xiaoBoardLedSet(color & 1, color & 2, color & 4);
        prevState = color;
    }
}
