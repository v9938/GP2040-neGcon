/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#include "storagemanager.h"

#include "BoardConfig.h"
#include "FlashPROM.h"
#include "drivermanager.h"
#include "eventmanager.h"
#include "peripheralmanager.h"
#include "config.pb.h"
#include "hardware/watchdog.h"
#include "CRC32.h"
#include "types.h"
#include <cmath>

// Check for saves
#include "ps4/PS4Driver.h"

#include "config_utils.h"
#include "addons/analog.h"

bool Storage::isPsxReservedPhysicalPin(int32_t pin) {
	// 仮想 slot 番号とは独立した、基板配線の予約。
	return (pin >= 0 && pin <= 4) || pin == 6 || pin == 7 ||
		pin == 11 || pin == 12 || pin == 16 || pin == 17 || pin == 25;
}

bool Storage::validatePsxLedPins(const Config& config) {
	const LEDOptions& leds = config.ledOptions;
	return leds.dataPin == -1 && leds.pledPin1 == -1 && leds.pledPin2 == -1 &&
		leds.pledPin3 == -1 && leds.pledPin4 == -1 && leds.pledType == PLED_TYPE_NONE &&
		leds.caseRGBType == CASE_RGB_TYPE_NONE;
}

bool Storage::isNormalButtonMask(uint32_t mask) {
	// 通常ボタン16個とdpad4方向。拡張入力やGPIO slot番号とは別のmask。
	return (mask & ~uint32_t(0x000fffff)) == 0;
}

bool Storage::validateMacroOptions(const MacroOptions& options) {
	if (options.macroList_count > sizeof(options.macroList) / sizeof(options.macroList[0])) return false;
	for (unsigned i = 0; i < options.macroList_count; ++i) {
		const auto& macro = options.macroList[i];
		if (macro.macroType < ON_PRESS || macro.macroType > ON_TOGGLE ||
			!isNormalButtonMask(macro.macroTriggerButton) ||
			macro.macroInputs_count > sizeof(macro.macroInputs) / sizeof(macro.macroInputs[0]) ||
			(macro.enabled && (macro.macroInputs_count == 0 ||
				(macro.useMacroTriggerButton && macro.macroTriggerButton == 0)))) return false;
		for (unsigned j = 0; j < macro.macroInputs_count; ++j) {
			const auto& input = macro.macroInputs[j];
			// 実行側の32bit保持時間へ渡す前に64bitで加算する。
			if (!isNormalButtonMask(input.buttonMask) ||
				uint64_t(input.duration) + input.waitDuration > UINT32_MAX) return false;
		}
	}
	return true;
}

bool Storage::isReactiveLedAction(int32_t action) {
	// 通常の18入力と8方向のアナログだけを対象にし、addon action は除外する。
	return action == NONE || (action >= BUTTON_PRESS_UP && action <= BUTTON_PRESS_R3) ||
		(action >= ANALOG_DIRECTION_LS_X_NEG && action <= ANALOG_DIRECTION_RS_Y_POS);
}

bool Storage::validateReactiveLedOptions(const ReactiveLEDOptions& options) {
	if (options.leds_count != 3) return false;
	for (unsigned channel = 0; channel < 3; ++channel) {
		const auto& led = options.leds[channel];
		if (led.pin != -1 || !isReactiveLedAction(led.action) ||
			led.modeDown < REACTIVE_LED_STATIC_OFF || led.modeDown > REACTIVE_LED_ANALOG ||
			led.modeUp < REACTIVE_LED_STATIC_OFF || led.modeUp > REACTIVE_LED_ANALOG ||
			led.brightness > 255) return false;
	}
	return true;
}

bool Storage::isPsxMappingAction(int32_t action) {
	// 列挙型の負値の穴や物理予約を仮想入力として受け入れない。
	return action == NONE || (action >= BUTTON_PRESS_UP && action <= MODE_WHEEL_PEDAL_CLUTCH);
}

bool Storage::validatePsxAnalogOptions(const AnalogOptions& a) {
	if (a.analogAdc1Mode < DPAD_MODE_DIGITAL || a.analogAdc1Mode > DPAD_MODE_RIGHT_ANALOG ||
		a.analogAdc2Mode < DPAD_MODE_DIGITAL || a.analogAdc2Mode > DPAD_MODE_RIGHT_ANALOG ||
		a.analogAdc1Invert < INVERT_NONE || a.analogAdc1Invert > INVERT_XY ||
		a.analogAdc2Invert < INVERT_NONE || a.analogAdc2Invert > INVERT_XY ||
		a.inner_deadzone >= a.outer_deadzone || a.outer_deadzone > 100 ||
		a.inner_deadzone2 >= a.outer_deadzone2 || a.outer_deadzone2 > 100 ||
		!std::isfinite(a.smoothing_factor) || a.smoothing_factor < 0 || a.smoothing_factor > 1000 ||
		!std::isfinite(a.smoothing_factor2) || a.smoothing_factor2 < 0 || a.smoothing_factor2 > 1000 ||
		a.analog_error > 1000 || a.analog_error2 > 1000 ||
		a.negconIMode > 14 || a.negconIIMode > 14 || a.negconLMode > 14 ||
		a.negconIThreshold > 255 || a.negconIIThreshold > 255 || a.negconLThreshold > 255 ||
		a.negconIInnerDeadzone >= a.negconIOuterDeadzone || a.negconIOuterDeadzone > 100 ||
		a.negconIIInnerDeadzone >= a.negconIIOuterDeadzone || a.negconIIOuterDeadzone > 100 ||
		a.negconLInnerDeadzone >= a.negconLOuterDeadzone || a.negconLOuterDeadzone > 100 ||
		a.jogconDeadzone > 32766 || a.jogconMax < 1 || a.jogconMax > 32767 ||
		a.jogconDeadzone >= a.jogconMax)
		return false;
	// DIGITAL は出力なし。同じ stick へ通常 DS の二組を重ねない。
	if (a.analogAdc1Mode != DPAD_MODE_DIGITAL && a.analogAdc1Mode == a.analogAdc2Mode)
		return false;
	// neGconの圧力同士・twistとの重複はすべて加算する。
	return true;
}

void Storage::enforcePsxConfig(Config& config) {
	static const GpioAction defaults[] = {
		BUTTON_PRESS_UP, BUTTON_PRESS_DOWN, BUTTON_PRESS_RIGHT, BUTTON_PRESS_LEFT,
		BUTTON_PRESS_B1, BUTTON_PRESS_B2, BUTTON_PRESS_R2, BUTTON_PRESS_L2,
		BUTTON_PRESS_B3, BUTTON_PRESS_B4, BUTTON_PRESS_R1, BUTTON_PRESS_L1,
		BUTTON_PRESS_S1, BUTTON_PRESS_S2, BUTTON_PRESS_L3, BUTTON_PRESS_R3
	};
	const bool migrate = !config.migrations.psxInputsMigrated;
	const auto constrainMappings = [&](GpioMappings& mappings) {
		for (unsigned slot = 0; slot < NUM_BANK0_GPIOS; ++slot) {
			if (migrate || slot < 8 || slot > 23 ||
				!isPsxMappingAction(mappings.pins[slot].action)) {
				mappings.pins[slot] = GpioMappingInfo GpioMappingInfo_init_zero;
				mappings.pins[slot].action = migrate && slot >= 8 && slot <= 23 ? defaults[slot - 8] : NONE;
			}
		}
		mappings.pins_count = NUM_BANK0_GPIOS;
	};
	// 旧 physical mapping は初回だけ PSX 配置に置換。以後の再起動・保存では保持。
	constrainMappings(config.gpioMappings);
	for (auto& mappings : config.profileOptions.gpioMappingsSets) constrainMappings(mappings);
	const unsigned profileLimit = sizeof(config.profileOptions.gpioMappingsSets) / sizeof(config.profileOptions.gpioMappingsSets[0]);
	if (config.profileOptions.gpioMappingsSets_count > profileLimit)
		config.profileOptions.gpioMappingsSets_count = profileLimit;
	if (config.gamepadOptions.profileNumber < 1 ||
		config.gamepadOptions.profileNumber > config.profileOptions.gpioMappingsSets_count + 1 ||
		(config.gamepadOptions.profileNumber > 1 && !config.profileOptions.gpioMappingsSets[config.gamepadOptions.profileNumber - 2].enabled))
		config.gamepadOptions.profileNumber = 1;
	config.migrations.psxInputsMigrated = true;
	config.migrations.has_psxInputsMigrated = true;

	AnalogOptions& analog = config.addonOptions.analogOptions;
	if (!analog.has_negconIMode) analog.negconIMode = 12;
	if (!analog.has_negconIIMode) analog.negconIIMode = 11;
	if (!analog.has_negconLMode) analog.negconLMode = 0;
	analog.has_negconIMode = analog.has_negconIIMode = analog.has_negconLMode = true;
	if (!analog.has_negconIInnerDeadzone) analog.negconIInnerDeadzone = 0;
	if (!analog.has_negconIOuterDeadzone) analog.negconIOuterDeadzone = 100;
	if (!analog.has_negconIIInnerDeadzone) analog.negconIIInnerDeadzone = 0;
	if (!analog.has_negconIIOuterDeadzone) analog.negconIIOuterDeadzone = 100;
	if (!analog.has_negconLInnerDeadzone) analog.negconLInnerDeadzone = 0;
	if (!analog.has_negconLOuterDeadzone) analog.negconLOuterDeadzone = 100;
	if (!analog.has_negconIDigitalEnabled) analog.negconIDigitalEnabled = true;
	if (!analog.has_negconIIDigitalEnabled) analog.negconIIDigitalEnabled = true;
	if (!analog.has_negconLDigitalEnabled) analog.negconLDigitalEnabled = true;
	analog.has_negconIInnerDeadzone = analog.has_negconIOuterDeadzone = true;
	analog.has_negconIIInnerDeadzone = analog.has_negconIIOuterDeadzone = true;
	analog.has_negconLInnerDeadzone = analog.has_negconLOuterDeadzone = true;
	analog.has_negconIDigitalEnabled = analog.has_negconIIDigitalEnabled = analog.has_negconLDigitalEnabled = true;
	if (!analog.has_jogconDeadzone) analog.jogconDeadzone = 0;
	if (!analog.has_jogconMax) analog.jogconMax = 100;
	if (!analog.has_jogconForcebackEnabled) analog.jogconForcebackEnabled = true;
	analog.has_jogconDeadzone = analog.has_jogconMax = analog.has_jogconForcebackEnabled = true;
	if (migrate) {
		analog.analogAdc1Mode = DPAD_MODE_LEFT_ANALOG;
		analog.analogAdc2Mode = DPAD_MODE_RIGHT_ANALOG;
	}
	analog.enabled = true;
	analog.analogAdc1PinX = analog.analogAdc1PinY = -1;
	analog.analogAdc2PinX = analog.analogAdc2PinY = -1;
	// 不正な保存値は起動時に安全な既定値へ戻す。API はこの補正の前に拒否する。
	if (!validatePsxAnalogOptions(analog)) {
		analog.analogAdc1Mode = DPAD_MODE_LEFT_ANALOG;
		analog.analogAdc2Mode = DPAD_MODE_RIGHT_ANALOG;
		analog.analogAdc1Invert = ANALOG_ADC_1_INVERT;
		analog.analogAdc2Invert = ANALOG_ADC_2_INVERT;
		analog.inner_deadzone = DEFAULT_INNER_DEADZONE;
		analog.inner_deadzone2 = DEFAULT_INNER_DEADZONE2;
		analog.outer_deadzone = DEFAULT_OUTER_DEADZONE;
		analog.outer_deadzone2 = DEFAULT_OUTER_DEADZONE2;
		analog.smoothing_factor = SMOOTHING_FACTOR;
		analog.smoothing_factor2 = SMOOTHING_FACTOR2;
		analog.analog_error = ANALOG_ERROR;
		analog.analog_error2 = ANALOG_ERROR2;
		analog.forced_circularity = !!FORCED_CIRCULARITY_ENABLED;
		analog.forced_circularity2 = !!FORCED_CIRCULARITY2_ENABLED;
		analog.auto_calibrate = !!AUTO_CALIBRATE_ENABLED;
		analog.auto_calibrate2 = !!AUTO_CALIBRATE2_ENABLED;
		analog.analog_smoothing = !!ANALOG_SMOOTHING_ENABLED;
		analog.analog_smoothing2 = !!ANALOG_SMOOTHING2_ENABLED;
		analog.negconIMode = 12;
		analog.negconIIMode = 11;
		analog.negconLMode = 0;
		analog.negconIThreshold = analog.negconIIThreshold = 128;
		analog.negconLThreshold = 240;
		analog.negconIInnerDeadzone = analog.negconIIInnerDeadzone = analog.negconLInnerDeadzone = 0;
		analog.negconIOuterDeadzone = analog.negconIIOuterDeadzone = analog.negconLOuterDeadzone = 100;
		analog.negconIDigitalEnabled = analog.negconIIDigitalEnabled = analog.negconLDigitalEnabled = true;
		analog.jogconDeadzone = 0;
		analog.jogconMax = 100;
		analog.jogconForcebackEnabled = true;
	}

	AddonOptions& addons = config.addonOptions;
	if (!isNormalButtonMask(addons.bootselButtonOptions.buttonMap)) addons.bootselButtonOptions.buttonMap = 0;
	addons.turboOptions.enabled = false;
	addons.deprecatedSliderOptions.enabled = false;
	addons.reverseOptions.ledPin = -1;
	addons.reverseOptions.buttonPin = -1;
	for (auto* action : {&addons.reverseOptions.actionUp, &addons.reverseOptions.actionDown,
		&addons.reverseOptions.actionLeft, &addons.reverseOptions.actionRight})
		if (*action > 2) *action = 0;
	addons.analogADS1219Options.enabled = false;
	addons.dualDirectionalOptions.enabled = false;
	addons.buzzerOptions.enabled = false;
	addons.deprecatedExtraButtonOptions.enabled = false;
	addons.playerNumberOptions.enabled = false;
	addons.wiiOptions.enabled = false;
	addons.socdSliderOptions.enabled = false;
	addons.snesOptions.enabled = false;
	addons.focusModeOptions.enabled = false;
	addons.tiltOptions.enabled = false;
	addons.macroOptions.macroBoardLedEnabled = false;
	addons.macroOptions.deprecatedPin = -1;
	for (auto& macro : addons.macroOptions.macroList) macro.deprecatedMacroTriggerPin = -1;
	if (!validateMacroOptions(addons.macroOptions)) {
		addons.macroOptions.enabled = false;
		for (auto& macro : addons.macroOptions.macroList) {
			macro.enabled = false;
			macro.macroType = ON_PRESS;
			macro.macroInputs_count = 0;
			macro.macroTriggerButton = 0;
		}
	}
	addons.macroOptions.macroList_count = 6;
	addons.inputHistoryOptions.deprecatedEnabled = false;
	addons.analogADS1256Options.enabled = false;
	addons.rotaryOptions.enabled = false;
	addons.pcf8575Options.enabled = false;
	addons.drv8833RumbleOptions.enabled = false;
	addons.tg16Options.enabled = false;
	addons.heTriggerOptions.enabled = false;
	config.displayOptions.enabled = false;

	PeripheralOptions& peripherals = config.peripheralOptions;
	peripherals.blockI2C0.enabled = peripherals.blockI2C1.enabled = false;
	peripherals.blockI2C0.sda = peripherals.blockI2C0.scl = -1;
	peripherals.blockI2C1.sda = peripherals.blockI2C1.scl = -1;
	peripherals.blockSPI0.enabled = peripherals.blockSPI1.enabled = false;
	peripherals.blockSPI0.rx = peripherals.blockSPI0.cs = peripherals.blockSPI0.sck = peripherals.blockSPI0.tx = -1;
	peripherals.blockSPI1.rx = peripherals.blockSPI1.cs = peripherals.blockSPI1.sck = peripherals.blockSPI1.tx = -1;
	peripherals.blockUSB0.enabled = true;
	peripherals.blockUSB0.dp = 6;
	peripherals.blockUSB0.order = 0;
	peripherals.blockUSB0.enable5v = -1;

	// 旧 NeoPico/player LED は固定 LED と共有させない。認証設定には触れない。
	LEDOptions& legacy = config.ledOptions;
	legacy.dataPin = legacy.pledPin1 = legacy.pledPin2 = legacy.pledPin3 = legacy.pledPin4 = -1;
	legacy.pledType = PLED_TYPE_NONE;
	legacy.caseRGBType = CASE_RGB_TYPE_NONE;
	config.animationOptions.hasCustomTheme = false;

	const bool migrateLeds = !config.migrations.xiaoLedsMigrated;
	auto& board = addons.onBoardLedOptions;
	auto& reactive = addons.reactiveLEDOptions;
	if (migrateLeds) {
		board.enabled = reactive.enabled = true;
		board.mode = ON_BOARD_LED_MODE_MODE_INDICATOR;
	}
	if (board.mode < ON_BOARD_LED_MODE_OFF || board.mode > ON_BOARD_LED_MODE_PS_AUTH)
		board.mode = ON_BOARD_LED_MODE_MODE_INDICATOR;
	static const GpioAction ledDefaults[] = {
		ANALOG_DIRECTION_LS_X_NEG, ANALOG_DIRECTION_LS_X_POS, ANALOG_DIRECTION_RS_Y_POS
	};
	// 保存容量10は旧データの復号用に残し、使用するチャンネルだけ3個に正規化する。
	for (unsigned channel = 0; channel < 10; ++channel) {
		auto& led = reactive.leds[channel];
		if (channel >= 3) {
			led = ReactiveLEDInfo ReactiveLEDInfo_init_zero;
			led.pin = -1;
			continue;
		}
		if (migrateLeds) {
			led = ReactiveLEDInfo ReactiveLEDInfo_init_default;
			led.action = ledDefaults[channel];
			led.modeDown = REACTIVE_LED_ANALOG;
			led.modeUp = REACTIVE_LED_STATIC_OFF;
			led.brightness = 255;
		}
		led.pin = -1;
		if (!isReactiveLedAction(led.action)) led.action = NONE;
		if (led.modeDown < REACTIVE_LED_STATIC_OFF || led.modeDown > REACTIVE_LED_ANALOG)
			led.modeDown = REACTIVE_LED_ANALOG;
		if (led.modeUp < REACTIVE_LED_STATIC_OFF || led.modeUp > REACTIVE_LED_ANALOG)
			led.modeUp = REACTIVE_LED_STATIC_OFF;
		if (!led.has_brightness || led.brightness > 255) led.brightness = 255;
		led.has_pin = led.has_action = led.has_modeDown = led.has_modeUp = led.has_brightness = true;
	}
	reactive.leds_count = 3;
	board.has_enabled = board.has_mode = reactive.has_enabled = true;
	addons.has_onBoardLedOptions = addons.has_reactiveLEDOptions = true;
	config.migrations.xiaoLedsMigrated = config.migrations.has_xiaoLedsMigrated = true;
}

void Storage::init() {
	systemFlashSize = System::getPhysicalFlash(); // System Flash Size must be called once
	EEPROM.start();
	ConfigUtils::load(config);
}

/**
 * @brief Save the config, but only if it is safe to (as in USB host is not being used.)
 */
bool Storage::save()
{
	return save(false);
}

/**
 * @brief Save the config; if forcing a save is requested, or if USB host is not enabled, this will write to flash.
 */
bool Storage::save(const bool force) {
	// Conditions for saving:
	//   1. Force = True
	//   2. Input Mode NOT (PS4/PS5 with USB enabled)
	// Save will disconnect USB host, which is okay for gamepad and keyboard hosts
	if (!force &&
		PeripheralManager::getInstance().isUSBEnabled(0) &&
		(DriverManager::getInstance().getInputMode() == INPUT_MODE_PS4 ||
			DriverManager::getInstance().getInputMode() == INPUT_MODE_PS5) &&
		((PS4Driver*)DriverManager::getInstance().getDriver())->getDongleAuthRequired() == true ) {
		return false;
	}

	return ConfigUtils::save(config);
}

void Storage::ResetSettings()
{
	// 再起動待ちの保存イベントでも旧設定を復活させず、PSX入力の既定値を維持する。
	config = Config Config_init_default;
	ConfigUtils::initUnsetPropertiesWithDefaults(config);
	enforcePsxConfig(config);
	EEPROM.reset();
	watchdog_reboot(0, SRAM_END, 2000);
}

bool Storage::setProfile(const uint32_t profileNum)
{
	// is this profile defined?
	if (profileNum >= 1 && profileNum <= config.profileOptions.gpioMappingsSets_count + 1) {
		// is this profile enabled?
		// profile 1 (core) is always enabled, others we must check
		if (profileNum == 1 || config.profileOptions.gpioMappingsSets[profileNum-2].enabled) {
			// Update the profile number - reinit will be triggered automatically in gp2040.cpp
			this->config.gamepadOptions.profileNumber = profileNum;
			return true;
		}
	}
	// if we get here, the requested profile doesn't exist or isn't enabled, so don't change it
	return false;
}

void Storage::nextProfile()
{
	uint32_t profileCeiling = config.profileOptions.gpioMappingsSets_count + 1;
	uint32_t requestedProfile = (this->config.gamepadOptions.profileNumber % profileCeiling) + 1;
	while (!setProfile(requestedProfile)) {
		// if the set failed, try again with the next in the sequence
		requestedProfile = (requestedProfile % profileCeiling) + 1;
	}
}
void Storage::previousProfile()
{
	uint32_t profileCeiling = config.profileOptions.gpioMappingsSets_count + 1;
	uint32_t requestedProfile = this->config.gamepadOptions.profileNumber > 1 ?
			config.gamepadOptions.profileNumber - 1 : profileCeiling;
	while (!setProfile(requestedProfile)) {
		// if the set failed, try again with the next in the sequence
		requestedProfile = requestedProfile > 1 ? requestedProfile - 1 : profileCeiling;
	}
}

/**
 * @brief Return the current profile label.
 */
char* Storage::currentProfileLabel() {
	if (this->config.gamepadOptions.profileNumber == 1)
		return this->config.gpioMappings.profileLabel;
	else
		return this->config.profileOptions.gpioMappingsSets[config.gamepadOptions.profileNumber-2].profileLabel;
}

void Storage::setFunctionalPinMappings()
{
	enforcePsxConfig(config);
	GpioMappingInfo* alts = nullptr;
	if (config.gamepadOptions.profileNumber >= 2 &&
			config.gamepadOptions.profileNumber <= config.profileOptions.gpioMappingsSets_count + 1) {
		if (config.profileOptions.gpioMappingsSets[config.gamepadOptions.profileNumber-2].enabled) {
			alts = config.profileOptions.gpioMappingsSets[config.gamepadOptions.profileNumber-2].pins;
		}
	}

	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
		// assign the functional pin to the profile pin if:
		// 1: there was a profile to load
		// 2: the new action isn't RESERVED or ASSIGNED_TO_ADDON (profiles can't affect special addons)
		// 3: the old action isn't RESERVED or ASSIGNED_TO_ADDON (profiles can't affect special addons)
		// else use whatever is in the core mapping
		if (alts != nullptr &&
				alts[pin].action != GpioAction::RESERVED &&
				alts[pin].action != GpioAction::ASSIGNED_TO_ADDON &&
				this->config.gpioMappings.pins[pin].action != GpioAction::RESERVED &&
				this->config.gpioMappings.pins[pin].action != GpioAction::ASSIGNED_TO_ADDON) {
			functionalPinMappings[pin] = alts[pin];
		} else {
			functionalPinMappings[pin] = this->config.gpioMappings.pins[pin];
		}
	}
}

void Storage::SetGamepad(Gamepad * newpad)
{
	gamepad = newpad;
}

Gamepad * Storage::GetGamepad()
{
	return gamepad;
}

void Storage::SetProcessedGamepad(Gamepad * newpad)
{
	processedGamepad = newpad;
}

Gamepad * Storage::GetProcessedGamepad()
{
	return processedGamepad;
}
