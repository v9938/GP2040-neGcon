#include "addons/bootsel_button.h"
#include "storagemanager.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "pico/flash.h"
#include "pico/multicore.h"

namespace {
void __no_inline_not_in_flash_func(sampleBootsel)(void* result) {
    const uint cs = 1;
    const uint32_t savedCtrl = ioqspi_hw->io[cs].ctrl;
    // flash_safe_execute has stopped core 1 and disabled local interrupts.
    hw_write_masked(&ioqspi_hw->io[cs].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int i = 0; i < 1000; ++i) {}
    const bool pressed = !(sio_hw->gpio_hi_in & (1u << cs));
    ioqspi_hw->io[cs].ctrl = savedCtrl;
    *static_cast<bool*>(result) = pressed;
}
}

bool BootselButtonAddon::available() {
    const auto& options = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    return options.enabled && options.buttonMap != 0;
}

void BootselButtonAddon::setup() {
    bootselButtonMap = Storage::getInstance().getAddonOptions().bootselButtonOptions.buttonMap;
}

void BootselButtonAddon::preprocess() {
    const auto& options = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    if (!options.enabled || !options.buttonMap) return;
    const uint64_t now = time_us_64();
    if (now >= nextSample) {
        nextSample = now + 5000;
        // Boot-action preprocessing also runs before core 1 has been launched.
        if (!multicore_lockout_victim_is_initialized(1)) return;
        bool pressed = false;
        if (flash_safe_execute(sampleBootsel, &pressed, 1) != PICO_OK) {
            stablePressed = false;
            sampledPressed = false;
            changedAt = now;
            return;
        }
        if (pressed != sampledPressed) {
            sampledPressed = pressed;
            changedAt = now;
        } else if (now - changedAt >= 10000) {
            stablePressed = sampledPressed;
        }
    }
    if (stablePressed) {
        Gamepad* gamepad = Storage::getInstance().GetGamepad();
        gamepad->state.dpad |= (bootselButtonMap >> 16) & GAMEPAD_MASK_DPAD;
        gamepad->state.buttons |= bootselButtonMap & 0xfff0ffffu;
    }
}
