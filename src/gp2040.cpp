// GP2040 includes
#include "gp2040.h"
#include "helper.h"
#include "system.h"
#include "enums.pb.h"

#include "build_info.h"
#include "peripheralmanager.h"
#include "storagemanager.h"
#include "addonmanager.h"
#include "types.h"
#include "psx_input.h"
#include "hardware/clocks.h"
#include "usbhostmanager.h"

// Inputs for Core0
#include "addons/analog.h"
#include "addons/bootsel_button.h"
#include "addons/keyboard_host.h"
#include "addons/reverse.h"
#include "addons/input_macro.h"
#include "addons/gamepad_usb_host.h"

// Pico includes
#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/adc.h"

#include "rndis.h"

void analogMonitorTick();

// TinyUSB
#include "tusb.h"

// USB Input Class Drivers
#include "drivermanager.h"

static const uint32_t REBOOT_HOTKEY_ACTIVATION_TIME_MS = 50;
static const uint32_t REBOOT_HOTKEY_HOLD_TIME_MS = 4000;

const static uint32_t rebootDelayMs = 500;
static absolute_time_t rebootDelayTimeout = nil_time;

void GP2040::setup() {
	Storage::getInstance().init();

	// Reduce CPU if USB host is enabled
	PeripheralManager::getInstance().initUSB();
	if ( PeripheralManager::getInstance().isUSBEnabled(0) ) {
		set_sys_clock_khz(120000, true); // Set Clock to 120MHz to avoid potential USB timing issues
	}

	// I2C & SPI rely on the system clock
	// PSXがSPI0を専有する。保存された外部SPI/I2C設定で固定配線を変更しない。
	PsxInput::getInstance().setup();

	Gamepad * gamepad = new Gamepad();
	Gamepad * processedGamepad = new Gamepad();
	Storage::getInstance().SetGamepad(gamepad);
	Storage::getInstance().SetProcessedGamepad(processedGamepad);

	// Set pin mappings for all GPIO functions
	Storage::getInstance().setFunctionalPinMappings();

	// power up...
	gamepad->auxState.power.pluggedIn = true;
	gamepad->auxState.power.charging = false;
	gamepad->auxState.power.level = GAMEPAD_AUX_MAX_POWER;

	// Setup Gamepad
	gamepad->setup();

	// Initialize last reinit profile to current so we don't reinit on first loop
	gamepad->lastReinitProfileNumber = Storage::getInstance().getGamepadOptions().profileNumber;

	// now we can load the latest configured profile, which will map the
	// new set of GPIOs to use...
	this->initializeStandardGpio();

	const GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();

	// check setup options and add modes to the list
	// user modes
	bootActions.insert({GAMEPAD_MASK_B1, gamepadOptions.inputModeB1});
	bootActions.insert({GAMEPAD_MASK_B2, gamepadOptions.inputModeB2});
	bootActions.insert({GAMEPAD_MASK_B3, gamepadOptions.inputModeB3});
	bootActions.insert({GAMEPAD_MASK_B4, gamepadOptions.inputModeB4});
	bootActions.insert({GAMEPAD_MASK_L1, gamepadOptions.inputModeL1});
	bootActions.insert({GAMEPAD_MASK_L2, gamepadOptions.inputModeL2});
	bootActions.insert({GAMEPAD_MASK_R1, gamepadOptions.inputModeR1});
	bootActions.insert({GAMEPAD_MASK_R2, gamepadOptions.inputModeR2});

	// Setup Add-ons
	addons.LoadAddon(new AnalogInput());
	// PSX軸を先に生成し、接続中のUSB PADの軸がAnalog処理で消されない順序にする。
	addons.LoadUSBAddon(new KeyboardHostAddon());
	addons.LoadUSBAddon(new GamepadUSBHostAddon());
	addons.LoadAddon(new BootselButtonAddon());
	addons.LoadAddon(new InputMacro());
	addons.LoadAddon(new ReverseInput());

	InputMode inputMode = gamepad->getOptions().inputMode;
	const BootAction bootAction = getBootAction();
	switch (bootAction) {
		case BootAction::ENTER_WEBCONFIG_MODE:
			inputMode = INPUT_MODE_CONFIG;
			break;
		case BootAction::ENTER_USB_MODE:
			reset_usb_boot(0, 0);
			return;
		case BootAction::SET_INPUT_MODE_SWITCH:
			inputMode = INPUT_MODE_SWITCH;
			break;
		case BootAction::SET_INPUT_MODE_KEYBOARD:
			inputMode = INPUT_MODE_KEYBOARD;
			break;
		case BootAction::SET_INPUT_MODE_GENERIC:
			inputMode = INPUT_MODE_GENERIC;
			break;
		case BootAction::SET_INPUT_MODE_NEOGEO:
			inputMode = INPUT_MODE_NEOGEO;
			break;
		case BootAction::SET_INPUT_MODE_MDMINI:
			inputMode = INPUT_MODE_MDMINI;
			break;
		case BootAction::SET_INPUT_MODE_PCEMINI:
			inputMode = INPUT_MODE_PCEMINI;
			break;
		case BootAction::SET_INPUT_MODE_EGRET:
			inputMode = INPUT_MODE_EGRET;
			break;
		case BootAction::SET_INPUT_MODE_ASTRO:
			inputMode = INPUT_MODE_ASTRO;
			break;
		case BootAction::SET_INPUT_MODE_PSCLASSIC:
			inputMode = INPUT_MODE_PSCLASSIC;
			break;
		case BootAction::SET_INPUT_MODE_XINPUT: // X-Input Driver
			inputMode = INPUT_MODE_XINPUT;
			break;
		case BootAction::SET_INPUT_MODE_PS3: // PS3 (HID with quirks) driver
			inputMode = INPUT_MODE_PS3;
			break;
		case BootAction::SET_INPUT_MODE_PS4: // PS4 / PS5 Driver
			inputMode = INPUT_MODE_PS4;
			break;
		case BootAction::SET_INPUT_MODE_PS5: // PS4 / PS5 Driver
			inputMode = INPUT_MODE_PS5;
			break;
		case BootAction::SET_INPUT_MODE_XBONE: // Xbox One Driver
			inputMode = INPUT_MODE_XBONE;
			break;
		case BootAction::SET_INPUT_MODE_XBOXORIGINAL: // Xbox OG Driver
			inputMode = INPUT_MODE_XBOXORIGINAL;
			break;
		case BootAction::SET_INPUT_MODE_SWITCH_PRO:
			inputMode = INPUT_MODE_SWITCH_PRO;
			break;
		case BootAction::SET_INPUT_MODE_XE1AJ:
			inputMode = INPUT_MODE_XE1AJ;
			break;
		case BootAction::NONE:
		default:
			break;
	}

	// Setup USB Driver
	DriverManager::getInstance().setup(inputMode);

	// save to match user expectations on choosing mode at boot, and this is
	// before USB host will be used so we can force it to ignore the check
	if (inputMode != INPUT_MODE_CONFIG && inputMode != gamepad->getOptions().inputMode) {
		gamepad->setInputMode(inputMode);
		Storage::getInstance().save(true);
	}

	// register system event handlers
	EventManager::getInstance().registerEventHandler(GP_EVENT_STORAGE_SAVE, GPEVENT_CALLBACK(this->handleStorageSave(event)));
	EventManager::getInstance().registerEventHandler(GP_EVENT_RESTART, GPEVENT_CALLBACK(this->handleSystemReboot(event)));
}

/**
 * @brief Initialize standard input button GPIOs that are present in the currently loaded profile.
 */
void GP2040::initializeStandardGpio() {
	GpioMappingInfo* pinMappings = Storage::getInstance().getProfilePinMappings();
	buttonGpios = 0;
	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
	{
		// (NONE=-10, RESERVED=-5, ASSIGNED_TO_ADDON=0, everything else is ours)
		if (pinMappings[pin].action > 0)
		{
			// ここでのpinはPSX入力の保存スロット。物理GPIOを操作しない。
			buttonGpios |= 1u << pin;
		}
	}
}

/**
 * @brief Deinitialize standard input button GPIOs that are present in the currently loaded profile.
 */
void GP2040::deinitializeStandardGpio() {
	buttonGpios = 0;
}

/**
 * @brief Populate a debounced version of gpio_get_all suitable for use for buttons.
 *
 * For GPIO that are assigned to buttons (based on GpioMappings, see GP2040::initializeStandardGpio),
 * we can centralize their debouncing here and provide access to it to button users.
 *
 * For ease of use this provides the mask bitwise NOTed so that callers don't have to. To avoid misuse
 * and to simplify this method, non-button GPIO IS NOT PRESENT in this result. Use gpio_get_all directly
 * instead, if you don't want debounced data.
 */
void GP2040::debounceGpioGetAll() {
	PsxInput::getInstance().poll();
	Gamepad* gamepad = Storage::getInstance().GetGamepad();
	// PSXパッド内でデバウンス済み。切断時も遅延なく全ボタンを解放する。
	gamepad->debouncedGpio = PsxInput::getInstance().virtualButtons() & buttonGpios;
}

void GP2040::run() {
	bool configMode = DriverManager::getInstance().isConfigMode();
	GPDriver * inputDriver = DriverManager::getInstance().getDriver();
	Gamepad * gamepad = Storage::getInstance().GetGamepad();
	Gamepad * processedGamepad = Storage::getInstance().GetProcessedGamepad();
	GamepadState prevState;

	// Start the TinyUSB Device functionality
	tud_init(TUD_OPT_RHPORT);

	// Initialize our USB manager
	USBHostManager::getInstance().start();

	if (configMode == true ) {
		rndis_init();
	}

	while (1) { // LOOP
		this->getReinitGamepad(gamepad);

		memcpy(&prevState, &gamepad->state, sizeof(GamepadState));

		// Debounce
		debounceGpioGetAll();
		// Read Gamepad
		gamepad->read();

		checkRawState(prevState, gamepad->state);

		// Process USB Host on Core0
		USBHostManager::getInstance().process();

		// Config Loop (Web-Config skips Core0 add-ons)
		if (configMode == true) {
			analogMonitorTick();
			inputDriver->process(gamepad);
			rebootHotkeys.process(gamepad, configMode);
			checkSaveRebootState();
			continue;
		}

		// Pre-Process add-ons for MPGS
		addons.PreprocessAddons();

		gamepad->hotkey(); 	// check for MPGS hotkeys
		rebootHotkeys.process(gamepad, configMode);

		gamepad->process(); // process through MPGS

		// (Post) Process for add-ons
		addons.ProcessAddons();

		checkProcessedState(processedGamepad->state, gamepad->state);

		// Copy Processed Gamepad for Core1 (race condition otherwise)
		memcpy(&processedGamepad->state, &gamepad->state, sizeof(GamepadState));

		// Process Input Driver
		bool processed = inputDriver->process(gamepad);

		// TinyUSB Task update
		tud_task();

		// Post-Process Add-ons with USB Report Processed Sent
		addons.PostprocessAddons(processed);

		// Check if we have a pending save
		checkSaveRebootState();
	}
}

void GP2040::getReinitGamepad(Gamepad * gamepad) {
	GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();

	// Check if profile has changed since last reinit
	if (gamepad->lastReinitProfileNumber != gamepadOptions.profileNumber) {
		uint32_t previousProfile = gamepad->lastReinitProfileNumber;
		uint32_t currentProfile = gamepadOptions.profileNumber;

		// deinitialize the ordinary (non-reserved, non-addon) GPIO pins, since
		// we are moving off of them and onto potentially different pin assignments
		// we currently don't support ASSIGNED_TO_ADDON pins being reinitialized,
		// but if they were to be, that'd be the addon's duty, not ours
		this->deinitializeStandardGpio();

		// now we can load the latest configured profile, which will map the
		// new set of GPIOs to use...
		Storage::getInstance().setFunctionalPinMappings();

		// ...and initialize the pins again
		this->initializeStandardGpio();

		// now we can tell the gamepad that the new mappings are in place
		// and ready to use, and the pins are ready, so it should reinitialize itself
		gamepad->reinit();

		// ...and addons on this core, if they implemented reinit (just things
		// with simple GPIO pin usage, at time of writing)
		addons.ReinitializeAddons();

		// Update the last reinit profile
		gamepad->lastReinitProfileNumber = currentProfile;

		// Trigger the profile change event now that reinit is complete
		EventManager::getInstance().triggerEvent(new GPProfileChangeEvent(previousProfile, currentProfile));
	}
}

GP2040::BootAction GP2040::getBootAction() {
	// 明示的な再起動要求は必ず消費する。START早期returnで古い要求を残さない。
	const System::BootMode bootMode = System::takeBootMode();
	if (bootMode == System::BootMode::USB) return BootAction::ENTER_USB_MODE;
	// 通常のボタン割当と無関係に物理STARTを判定し、リマップ後も設定へ戻れる。
	// 給電直後のパッド起動を待つ間だけポーリングする。通常ループには待機を入れない。
	const uint64_t deadline = time_us_64() + 3000000;
	while (time_us_64() < deadline) {
		PsxInput::getInstance().poll();
		if (PsxInput::getInstance().startPressed()) return BootAction::ENTER_WEBCONFIG_MODE;
		sleep_ms(1);
	}
	switch (bootMode) {
		case System::BootMode::GAMEPAD: return BootAction::NONE;
		case System::BootMode::WEBCONFIG: return BootAction::ENTER_WEBCONFIG_MODE;
		case System::BootMode::USB: return BootAction::ENTER_USB_MODE;
		case System::BootMode::DEFAULT:
			{
				// Determine boot action based on gamepad state during boot
				Gamepad * gamepad = Storage::getInstance().GetGamepad();
				Gamepad * processedGamepad = Storage::getInstance().GetProcessedGamepad();

				debounceGpioGetAll();
				gamepad->read();

				// Pre-Process add-ons for MPGS
				addons.PreprocessAddons();

				gamepad->process(); // process through MPGS

				// Process for add-ons
				addons.ProcessAddons();

				// Copy Processed Gamepad for Core1 (race condition otherwise)
				memcpy(&processedGamepad->state, &gamepad->state, sizeof(GamepadState));

                const ForcedSetupOptions& forcedSetupOptions = Storage::getInstance().getForcedSetupOptions();
                bool modeSwitchLocked = forcedSetupOptions.mode == FORCED_SETUP_MODE_LOCK_MODE_SWITCH ||
                                        forcedSetupOptions.mode == FORCED_SETUP_MODE_LOCK_BOTH;

                bool webConfigLocked  = forcedSetupOptions.mode == FORCED_SETUP_MODE_LOCK_WEB_CONFIG ||
                                        forcedSetupOptions.mode == FORCED_SETUP_MODE_LOCK_BOTH;

				if (gamepad->pressedS1() && gamepad->pressedS2() && gamepad->pressedUp()) {
					return BootAction::ENTER_USB_MODE;
				} else if (!webConfigLocked && PsxInput::getInstance().startPressed()) {
					return BootAction::ENTER_WEBCONFIG_MODE;
                } else {
                    if (!modeSwitchLocked) {
                        if (auto search = bootActions.find(gamepad->state.buttons); search != bootActions.end()) {
                            switch (search->second) {
                                case INPUT_MODE_XINPUT:
                                    return BootAction::SET_INPUT_MODE_XINPUT;
                                case INPUT_MODE_SWITCH:
                                    return BootAction::SET_INPUT_MODE_SWITCH;
                                case INPUT_MODE_KEYBOARD:
                                    return BootAction::SET_INPUT_MODE_KEYBOARD;
                                case INPUT_MODE_GENERIC:
                                    return BootAction::SET_INPUT_MODE_GENERIC;
                                case INPUT_MODE_PS3:
                                    return BootAction::SET_INPUT_MODE_PS3;
                                case INPUT_MODE_PS4:
                                    return BootAction::SET_INPUT_MODE_PS4;
                                case INPUT_MODE_PS5:
                                    return BootAction::SET_INPUT_MODE_PS5;
                                case INPUT_MODE_NEOGEO:
                                    return BootAction::SET_INPUT_MODE_NEOGEO;
                                case INPUT_MODE_MDMINI:
                                    return BootAction::SET_INPUT_MODE_MDMINI;
                                case INPUT_MODE_PCEMINI:
                                    return BootAction::SET_INPUT_MODE_PCEMINI;
                                case INPUT_MODE_EGRET:
                                    return BootAction::SET_INPUT_MODE_EGRET;
                                case INPUT_MODE_ASTRO:
                                    return BootAction::SET_INPUT_MODE_ASTRO;
                                case INPUT_MODE_PSCLASSIC:
                                    return BootAction::SET_INPUT_MODE_PSCLASSIC;
                                case INPUT_MODE_XBOXORIGINAL:
                                    return BootAction::SET_INPUT_MODE_XBOXORIGINAL;
                                case INPUT_MODE_XBONE:
                                    return BootAction::SET_INPUT_MODE_XBONE;
                                case INPUT_MODE_SWITCH_PRO:
                                    return BootAction::SET_INPUT_MODE_SWITCH_PRO;
                                case INPUT_MODE_XE1AJ:
                                    return BootAction::SET_INPUT_MODE_XE1AJ;
                                default:
                                    return BootAction::NONE;
                            }
                        }
                    }
                }

				break;
			}
	}

	return BootAction::NONE;
}

GP2040::RebootHotkeys::RebootHotkeys() :
	active(false),
	noButtonsPressedTimeout(nil_time),
	webConfigHotkeyMask(GAMEPAD_MASK_S2 | GAMEPAD_MASK_B3 | GAMEPAD_MASK_B4),
	bootselHotkeyMask(GAMEPAD_MASK_S1 | GAMEPAD_MASK_B3 | GAMEPAD_MASK_B4),
	rebootHotkeysHoldTimeout(nil_time) {
}

void GP2040::RebootHotkeys::process(Gamepad* gamepad, bool configMode) {
	// We only allow the hotkey to trigger after we observed no buttons pressed for a certain period of time.
	// We do this to avoid detecting buttons that are held during the boot process. In particular we want to avoid
	// oscillating between webconfig and default mode when the user keeps holding the hotkey buttons.
	if (!active) {
		if (gamepad->state.buttons == 0) {
			if (is_nil_time(noButtonsPressedTimeout)) {
				noButtonsPressedTimeout = make_timeout_time_us(REBOOT_HOTKEY_ACTIVATION_TIME_MS);
			}

			if (time_reached(noButtonsPressedTimeout)) {
				active = true;
			}
		} else {
			noButtonsPressedTimeout = nil_time;
		}
	} else {
		if (gamepad->state.buttons == webConfigHotkeyMask || gamepad->state.buttons == bootselHotkeyMask) {
			if (is_nil_time(rebootHotkeysHoldTimeout)) {
				rebootHotkeysHoldTimeout = make_timeout_time_ms(REBOOT_HOTKEY_HOLD_TIME_MS);
			}

			if (time_reached(rebootHotkeysHoldTimeout)) {
				if (gamepad->state.buttons == webConfigHotkeyMask) {
					// If we are in webconfig mode we go to gamepad mode and vice versa
					System::reboot(configMode ? System::BootMode::GAMEPAD : System::BootMode::WEBCONFIG);
				} else if (gamepad->state.buttons == bootselHotkeyMask) {
					System::reboot(System::BootMode::USB);
				}
			}
		} else {
			rebootHotkeysHoldTimeout = nil_time;
		}
	}
}

void GP2040::checkRawState(GamepadState prevState, GamepadState currState) {
    // buttons pressed
    if (
        ((currState.aux & ~prevState.aux) != 0) ||
        ((currState.dpad & ~prevState.dpad) != 0) ||
        ((currState.buttons & ~prevState.buttons) != 0)
    ) {
        EventManager::getInstance().triggerEvent(new GPButtonDownEvent((currState.dpad & ~prevState.dpad), (currState.buttons & ~prevState.buttons), (currState.aux & ~prevState.aux)));
    }

    // buttons released
    if (
        ((prevState.aux & ~currState.aux) != 0) ||
        ((prevState.dpad & ~currState.dpad) != 0) ||
        ((prevState.buttons & ~currState.buttons) != 0)
    ) {
        EventManager::getInstance().triggerEvent(new GPButtonUpEvent((prevState.dpad & ~currState.dpad), (prevState.buttons & ~currState.buttons), (prevState.aux & ~currState.aux)));
    }
}

void GP2040::checkProcessedState(GamepadState prevState, GamepadState currState) {
    // buttons pressed
    if (
        ((currState.aux & ~prevState.aux) != 0) ||
        ((currState.dpad & ~prevState.dpad) != 0) ||
        ((currState.buttons & ~prevState.buttons) != 0)
    ) {
        EventManager::getInstance().triggerEvent(new GPButtonProcessedDownEvent((currState.dpad & ~prevState.dpad), (currState.buttons & ~prevState.buttons), (currState.aux & ~prevState.aux)));
    }

    // buttons released
    if (
        ((prevState.aux & ~currState.aux) != 0) ||
        ((prevState.dpad & ~currState.dpad) != 0) ||
        ((prevState.buttons & ~currState.buttons) != 0)
    ) {
        EventManager::getInstance().triggerEvent(new GPButtonProcessedUpEvent((prevState.dpad & ~currState.dpad), (prevState.buttons & ~currState.buttons), (prevState.aux & ~currState.aux)));
    }

    if (
        (currState.lx != prevState.lx) ||
        (currState.ly != prevState.ly) ||
        (currState.rx != prevState.rx) ||
        (currState.ry != prevState.ry) ||
        (currState.lt != prevState.lt) ||
        (currState.rt != prevState.rt)
    ) {
        EventManager::getInstance().triggerEvent(new GPAnalogProcessedMoveEvent(currState.lx, currState.ly, currState.rx, currState.ry, currState.lt, currState.rt));
    }
}

void GP2040::checkSaveRebootState() {
	if (saveRequested) {
		saveRequested = false;
		Storage::getInstance().save(forceSave);
	}

	if (rebootRequested) {
		rebootRequested = false;
		rebootDelayTimeout = make_timeout_time_ms(rebootDelayMs);
	}

	if (!is_nil_time(rebootDelayTimeout) && time_reached(rebootDelayTimeout)) {
		System::reboot(rebootMode);
	}
}

void GP2040::handleStorageSave(GPEvent* e) {
	saveRequested = true;
	forceSave = ((GPStorageSaveEvent*)e)->forceSave;
	rebootRequested = ((GPStorageSaveEvent*)e)->restartAfterSave;
	rebootMode = System::BootMode::DEFAULT;
}

void GP2040::handleSystemReboot(GPEvent* e) {
	rebootRequested = true;
	rebootMode = ((GPRestartEvent*)e)->bootMode;
}
