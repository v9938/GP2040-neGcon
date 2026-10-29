import { BUTTON_ACTIONS } from '../Data/Pins';

export type PsxPreset = {
	profile: { profileLabel: string; enabled: boolean; source: number };
	mapping: Record<string, { action: number; customButtonMask: number; customDpadMask: number }>;
	addons: Record<string, unknown>;
	reactiveLEDs: { leds: any[] };
	macros: { macroList: any[] };
};

export const PRESET_MAX_BYTES = 65536;
export const PSX_PRESET_INPUTS = ['UP', 'DOWN', 'RIGHT', 'LEFT', 'CROSS', 'CIRCLE', 'R2', 'L2', 'SQUARE', 'TRIANGLE', 'R1', 'L1', 'SELECT', 'START', 'L3', 'R3'] as const;
export const PRESET_KEYBOARD_BUTTONS = ['Up', 'Down', 'Left', 'Right', 'B1', 'B2', 'B3', 'B4', 'L1', 'R1', 'L2', 'R2', 'S1', 'S2', 'L3', 'R3', 'A1', 'A2', 'A3', 'A4', 'E1', 'E2', 'E3', 'E4', 'E5', 'E6', 'E7', 'E8', 'E9', 'E10', 'E11', 'E12', 'Fn'];

// 固定配線・認証・USB識別子は許可リストに含めない。
export const PRESET_ADDON_RULES: Record<string, { min?: number; max?: number; choices?: number[]; decimal?: boolean }> = {
	analogAdc1Mode: { choices: [1, 2] }, analogAdc2Mode: { choices: [1, 2] },
	analogAdc1Invert: { min: 0, max: 3 }, analogAdc2Invert: { min: 0, max: 3 },
	negconIMode: { min: 0, max: 14 }, negconIIMode: { min: 0, max: 14 }, negconLMode: { min: 0, max: 14 },
	negconIThreshold: { min: 0, max: 255 }, negconIIThreshold: { min: 0, max: 255 }, negconLThreshold: { min: 0, max: 255 },
	negconIDigitalEnabled: { choices: [0, 1] }, negconIIDigitalEnabled: { choices: [0, 1] }, negconLDigitalEnabled: { choices: [0, 1] },
	negconIInnerDeadzone: { min: 0, max: 100 }, negconIIInnerDeadzone: { min: 0, max: 100 }, negconLInnerDeadzone: { min: 0, max: 100 },
	negconIOuterDeadzone: { min: 0, max: 100 }, negconIIOuterDeadzone: { min: 0, max: 100 }, negconLOuterDeadzone: { min: 0, max: 100 },
	jogconDeadzone: { min: 0, max: 32766 }, jogconMax: { min: 1, max: 32767 }, jogconForcebackEnabled: { choices: [0, 1] },
	forced_circularity: { choices: [0, 1] }, forced_circularity2: { choices: [0, 1] },
	inner_deadzone: { min: 0, max: 100 }, inner_deadzone2: { min: 0, max: 100 },
	outer_deadzone: { min: 0, max: 100 }, outer_deadzone2: { min: 0, max: 100 },
	auto_calibrate: { choices: [0, 1] }, auto_calibrate2: { choices: [0, 1] },
	analog_smoothing: { choices: [0, 1] }, analog_smoothing2: { choices: [0, 1] },
	smoothing_factor: { min: 0, max: 100, decimal: true }, smoothing_factor2: { min: 0, max: 100, decimal: true },
	analog_error: { choices: [1000, 990, 979, 969, 958, 946, 934, 922, 911, 900, 890, 876, 863, 848, 834, 821] },
	analog_error2: { choices: [1000, 990, 979, 969, 958, 946, 934, 922, 911, 900, 890, 876, 863, 848, 834, 821] },
	BootselButtonAddonEnabled: { choices: [0, 1] }, bootselButtonMap: { min: 0, max: 1048575 },
	BoardLedAddonEnabled: { choices: [0, 1] }, onBoardLedMode: { min: 0, max: 3 },
	ReverseInputEnabled: { choices: [0, 1] },
	reverseActionUp: { min: 0, max: 2 }, reverseActionDown: { min: 0, max: 2 }, reverseActionLeft: { min: 0, max: 2 }, reverseActionRight: { min: 0, max: 2 },
	KeyboardHostAddonEnabled: { choices: [0, 1] }, GamepadUSBHostAddonEnabled: { choices: [0, 1] },
	keyboardHostMouseLeft: { min: 0, max: 4294967295 }, keyboardHostMouseMiddle: { min: 0, max: 4294967295 }, keyboardHostMouseRight: { min: 0, max: 4294967295 },
	keyboardHostMouseSensitivity: { min: 0, max: 100, decimal: true }, keyboardHostMouseMovement: { min: 0, max: 2 },
	ReactiveLEDAddonEnabled: { choices: [0, 1] }, InputMacroAddonEnabled: { choices: [0, 1] },
};

type Scalar = string | number | boolean;
type Sections = Record<string, Record<string, Scalar>>;
const jogconDefaults = { jogconDeadzone: 0, jogconMax: 100, jogconForcebackEnabled: 1 };
export const NEGCON_PRESSURE_DEFAULTS = Object.fromEntries(['I', 'II', 'L'].flatMap((input) => [
	[`negcon${input}DigitalEnabled`, 1], [`negcon${input}InnerDeadzone`, 0], [`negcon${input}OuterDeadzone`, 100],
])) as Record<`negcon${'I' | 'II' | 'L'}${'DigitalEnabled' | 'InnerDeadzone' | 'OuterDeadzone'}`, number>;
const actionNames = Object.fromEntries(Object.entries(BUTTON_ACTIONS).map(([name, value]) => [value, name]));
const macroFlags = ['enabled', 'exclusive', 'interruptible', 'showFrames', 'useMacroTriggerButton'];
const fail = (message: string): never => { throw new Error(message); };

function checkKeys(value: unknown, keys: string[], path: string): asserts value is Record<string, any> {
	if (!value || typeof value !== 'object' || Array.isArray(value)) fail(`${path}: オブジェクトが必要です`);
	const actual = Object.keys(value as object);
	if (actual.length !== keys.length || actual.some((key) => !keys.includes(key))) fail(`${path}: 必須項目不足または未知の項目があります`);
}

function number(value: unknown, path: string, min: number, max: number, decimal = false): number {
	if (typeof value !== 'number' || !Number.isFinite(value) || (!decimal && !Number.isInteger(value)) || value < min || value > max) fail(`${path}: 数値範囲 ${min}..${max} が必要です`);
	return value as number;
}

function checkString(value: unknown, path: string, maxBytes: number) {
	if (typeof value !== 'string' || /[\u0000-\u001f\u007f]/.test(value) || /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]/u.test(value) || new TextEncoder().encode(value).length > maxBytes) fail(`${path}: 文字列が不正です`);
}

function action(value: unknown, reactive = false): number {
	if (typeof value !== 'number' || !Object.hasOwn(actionNames, value) || value === BUTTON_ACTIONS.RESERVED || value === BUTTON_ACTIONS.ASSIGNED_TO_ADDON || (reactive && value !== BUTTON_ACTIONS.NONE && !(value >= 1 && value <= 18) && !(value >= 59 && value <= 66))) fail('action: 許可されていないアクションです');
	return value as number;
}

export function validatePreset(preset: PsxPreset): PsxPreset {
	checkKeys(preset, ['profile', 'mapping', 'addons', 'reactiveLEDs', 'macros'], 'Preset');
	checkKeys(preset.profile, ['profileLabel', 'enabled', 'source'], 'Profile');
	checkString(preset.profile.profileLabel, 'Profile.label', 16);
	if (!/^[a-zA-Z0-9 ]*$/.test(preset.profile.profileLabel) || typeof preset.profile.enabled !== 'boolean') fail('Profile: ラベルまたはenabledが不正です');
	number(preset.profile.source, 'Profile.source', 1, 6);
	checkKeys(preset.mapping, [...PSX_PRESET_INPUTS], 'Mapping');
	for (const [key, mapping] of Object.entries(preset.mapping)) {
		checkKeys(mapping, ['action', 'customButtonMask', 'customDpadMask'], `Mapping.${key}`);
		action(mapping.action);
		number(mapping.customButtonMask, `${key}.buttons`, 0, 65535);
		number(mapping.customDpadMask, `${key}.dpad`, 0, 15);
		if (mapping.action !== BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO && (mapping.customButtonMask || mapping.customDpadMask)) fail(`${key}: CUSTOM_BUTTON_COMBO以外のマスクは0にしてください`);
	}
	checkKeys(preset.addons, [...Object.keys(PRESET_ADDON_RULES), 'keyboardHostMap'], 'Addons');
	for (const [key, rule] of Object.entries(PRESET_ADDON_RULES)) {
		const value = number(preset.addons[key], `Addons.${key}`, rule.min ?? 0, rule.max ?? 4294967295, rule.decimal);
		if (rule.choices && !rule.choices.includes(value)) fail(`Addons.${key}: 列挙値が不正です`);
	}
	const a = preset.addons as Record<string, number>;
	if (a.jogconDeadzone >= a.jogconMax) fail('Addons: JOGCONのデッドゾーンは最大値より小さくしてください');
	if (a.analogAdc1Mode === a.analogAdc2Mode || a.outer_deadzone <= a.inner_deadzone || a.outer_deadzone2 <= a.inner_deadzone2) fail('Addons: スティック出力またはデッドゾーンが重複しています');
	for (const input of ['I', 'II', 'L']) {
		if (a[`negcon${input}InnerDeadzone`] >= a[`negcon${input}OuterDeadzone`]) fail(`Addons: neGcon ${input}のinnerはouterより小さくしてください`);
	}
	for (const key of ['bootselButtonMap', 'keyboardHostMouseLeft', 'keyboardHostMouseMiddle', 'keyboardHostMouseRight']) {
		const mask = a[key];
		if (mask !== 0 && (mask & (mask - 1)) !== 0) fail(`Addons.${key}: 単一ボタンのマスクが必要です`);
	}
	checkKeys(preset.addons.keyboardHostMap, PRESET_KEYBOARD_BUTTONS, 'KeyboardHostMap');
	for (const [key, value] of Object.entries(preset.addons.keyboardHostMap as Record<string, unknown>)) number(value, `KeyboardHostMap.${key}`, 0, 255);
	checkKeys(preset.reactiveLEDs, ['leds'], 'Reactive');
	if (!Array.isArray(preset.reactiveLEDs.leds) || preset.reactiveLEDs.leds.length !== 3) fail('Reactive: RGBの3チャンネルが必要です');
	for (const led of preset.reactiveLEDs.leds) {
		checkKeys(led, ['action', 'modeUp', 'modeDown', 'brightness'], 'Reactive');
		action(led.action, true);
		number(led.modeUp, 'Reactive.modeUp', 0, 4); number(led.modeDown, 'Reactive.modeDown', 0, 4); number(led.brightness, 'Reactive.brightness', 0, 255);
	}
	checkKeys(preset.macros, ['macroList'], 'Macros');
	if (!Array.isArray(preset.macros.macroList) || preset.macros.macroList.length !== 6) fail('Macro: 6件が必要です');
	for (const macro of preset.macros.macroList) {
		checkKeys(macro, ['macroLabel', 'macroType', ...macroFlags, 'macroTriggerButton', 'macroInputs'], 'Macro');
		checkString(macro.macroLabel, 'Macro.macroLabel', 64);
		number(macro.macroType, 'Macro.macroType', 1, 3);
		macroFlags.forEach((key) => number(macro[key], `Macro.${key}`, 0, 1));
		number(macro.macroTriggerButton, 'Macro.macroTriggerButton', 0, 1048575);
		if (!Array.isArray(macro.macroInputs) || macro.macroInputs.length > 30) fail('Macro.inputCount: 0..30が必要です');
		if (macro.enabled && (!macro.macroInputs.length || (macro.useMacroTriggerButton && !macro.macroTriggerButton))) fail('Macro: 有効なMacroには入力と有効なトリガーが必要です');
		for (const input of macro.macroInputs) {
			checkKeys(input, ['buttonMask', 'duration', 'waitDuration'], 'Macro.Input');
			Object.entries(input).forEach(([key, value]) => number(value, `Macro.Input.${key}`, 0, 4294967295));
			number(input.buttonMask, 'Macro.Input.buttonMask', 0, 1048575);
			if (input.duration + input.waitDuration > 4294967295) fail('Macro.Input: duration + waitDurationがuint32範囲を超えています');
		}
	}
	return preset;
}

export function serializePreset(preset: PsxPreset): string {
	validatePreset(preset);
	const sections: Sections = {
		Preset: { format: 'GP2040-PSX-PRESET', version: 3 },
		Profile: { label: preset.profile.profileLabel, enabled: preset.profile.enabled, source: preset.profile.source },
		Mapping: Object.fromEntries(PSX_PRESET_INPUTS.map((key) => [key, actionNames[preset.mapping[key].action]])),
		MappingMasks: Object.fromEntries(PSX_PRESET_INPUTS.flatMap((key) => [[`${key}.buttons`, preset.mapping[key].customButtonMask], [`${key}.dpad`, preset.mapping[key].customDpadMask]])),
		Addons: Object.fromEntries(Object.keys(PRESET_ADDON_RULES).map((key) => [key, preset.addons[key] as number])),
		KeyboardHostMap: preset.addons.keyboardHostMap as Record<string, number>,
	};
	preset.reactiveLEDs.leds.forEach((led, i) => { sections[`Reactive.${i}`] = { action: actionNames[led.action], modeUp: led.modeUp, modeDown: led.modeDown, brightness: led.brightness }; });
	preset.macros.macroList.forEach(({ macroInputs, ...macro }, i) => {
		sections[`Macro.${i}`] = { ...macro, inputCount: macroInputs.length };
		macroInputs.forEach((input: Record<string, number>, j: number) => { sections[`Macro.${i}.Input.${j}`] = input; });
	});
	const text = '# GP2040 PSX preset v3: shared ADDON settings apply to all profiles.\n# neGcon pressure adds to existing outputs; legacy full behavior changes. Migration preserves modes, including 14/13.\n' + Object.entries(sections).map(([section, values]) => `[${section}]\n${Object.entries(values).map(([key, value]) => `${key} = ${typeof value === 'string' ? JSON.stringify(value) : String(value)}`).join('\n')}\n`).join('\n');
	if (new TextEncoder().encode(text).length > PRESET_MAX_BYTES) fail('TXT: 64KiBを超えています');
	return text;
}

export function parsePreset(text: string): PsxPreset {
	if (new TextEncoder().encode(text).length > PRESET_MAX_BYTES) fail('TXT: 64KiBを超えています');
	if (/\r(?!\n)/.test(text)) fail('TXT: 改行はLFまたはCRLFを使用してください');
	const sections: Sections = Object.create(null);
	let current: Record<string, Scalar> | undefined;
	// JSONの使用は引用文字列の復号だけ。配列・オブジェクトは受け付けない。
	text.replace(/^\uFEFF/, '').split(/\r\n|\n/).forEach((raw, i) => {
		const line = raw.replace(/^[ \t]+|[ \t]+$/g, '');
		if (!line || line.startsWith('#')) return;
		const header = /^\[([A-Za-z][A-Za-z0-9.]*)\]$/.exec(line);
		if (header) {
			if (Object.hasOwn(sections, header[1])) fail(`行${i + 1}: セクションが重複しています`);
			current = sections[header[1]] = Object.create(null); return;
		}
		const entry = /^([A-Za-z][A-Za-z0-9_.]*)[ \t]*=[ \t]*(.+)$/.exec(line);
		if (!entry || !current || Object.hasOwn(current, entry[1])) fail(`行${i + 1}: 項目の構文または重複が不正です`);
		const [, key, rawValue] = entry!;
		let value: Scalar;
		if (/^"(?:[^"\\\u0000-\u001f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"$/.test(rawValue)) value = JSON.parse(rawValue);
		else if (rawValue === 'true' || rawValue === 'false') value = rawValue === 'true';
		else if (/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?$/.test(rawValue) || /^0x[0-9a-fA-F]+$/.test(rawValue)) value = Number(rawValue);
		else fail(`行${i + 1}: 数値・bool・引用文字列だけ使用できます`);
		current![key] = value!;
	});
	const take = (name: string, keys: string[]) => { const section = sections[name]; checkKeys(section, keys, name); delete sections[name]; return section; };
	const metadata = take('Preset', ['format', 'version']);
	if (metadata.format !== 'GP2040-PSX-PRESET' || ![1, 2, 3].includes(metadata.version as number)) fail('Preset: format/versionが未対応です');
	const p = take('Profile', ['label', 'enabled', 'source']);
	const mapping = take('Mapping', [...PSX_PRESET_INPUTS]);
	const masks = take('MappingMasks', PSX_PRESET_INPUTS.flatMap((key) => [`${key}.buttons`, `${key}.dpad`]));
	const readAction = (name: unknown): number => { if (typeof name !== 'string' || !Object.hasOwn(BUTTON_ACTIONS, name)) fail('Mapping/Reactive: 未知のアクション名です'); return BUTTON_ACTIONS[name as keyof typeof BUTTON_ACTIONS] as number; };
	const addonKeys = Object.keys(PRESET_ADDON_RULES).filter((key) =>
		(metadata.version !== 1 || !Object.hasOwn(jogconDefaults, key)) &&
		(metadata.version === 3 || !Object.hasOwn(NEGCON_PRESSURE_DEFAULTS, key)));
	const addons = { ...take('Addons', addonKeys), ...(metadata.version === 1 ? jogconDefaults : {}),
		...(metadata.version !== 3 ? NEGCON_PRESSURE_DEFAULTS : {}), keyboardHostMap: take('KeyboardHostMap', PRESET_KEYBOARD_BUTTONS) };
	const leds = Array.from({ length: 3 }, (_, i) => { const led = take(`Reactive.${i}`, ['action', 'modeUp', 'modeDown', 'brightness']); return { ...led, action: readAction(led.action) }; });
	const macroList = Array.from({ length: 6 }, (_, i) => {
		const { inputCount, ...macro } = take(`Macro.${i}`, ['macroLabel', 'macroType', ...macroFlags, 'macroTriggerButton', 'inputCount']);
		const count = number(inputCount, `Macro.${i}.inputCount`, 0, 30);
		return { ...macro, macroInputs: Array.from({ length: count }, (_, j) => take(`Macro.${i}.Input.${j}`, ['buttonMask', 'duration', 'waitDuration'])) };
	});
	if (Object.keys(sections).length) fail(`TXT: 未知のセクション ${Object.keys(sections).join(', ')}`);
	return validatePreset({
		profile: { profileLabel: p.label, enabled: p.enabled, source: p.source },
		mapping: Object.fromEntries(PSX_PRESET_INPUTS.map((key) => [key, { action: readAction(mapping[key]), customButtonMask: masks[`${key}.buttons`], customDpadMask: masks[`${key}.dpad`] }])),
		addons, reactiveLEDs: { leds }, macros: { macroList },
	} as PsxPreset);
}
