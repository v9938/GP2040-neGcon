import { InputMode } from '@proto/enums';
import { BUTTON_ACTIONS } from './Pins';
import { PSX_INPUTS } from './PSX';
import type { PinsType } from '../Store/useProfilesStore';

// proto/enums.proto の action 番号と GamepadState.h の mask は別の順序。
export const MODE_BUTTON_ACTIONS = [
	{ key: 'Up', action: 1, dpadMask: 1, buttonMask: 0 },
	{ key: 'Down', action: 2, dpadMask: 2, buttonMask: 0 },
	{ key: 'Left', action: 3, dpadMask: 4, buttonMask: 0 },
	{ key: 'Right', action: 4, dpadMask: 8, buttonMask: 0 },
	{ key: 'B1', action: 5, buttonMask: 1, dpadMask: 0 },
	{ key: 'B2', action: 6, buttonMask: 2, dpadMask: 0 },
	{ key: 'B3', action: 7, buttonMask: 4, dpadMask: 0 },
	{ key: 'B4', action: 8, buttonMask: 8, dpadMask: 0 },
	{ key: 'L1', action: 9, buttonMask: 16, dpadMask: 0 },
	{ key: 'R1', action: 10, buttonMask: 32, dpadMask: 0 },
	{ key: 'L2', action: 11, buttonMask: 64, dpadMask: 0 },
	{ key: 'R2', action: 12, buttonMask: 128, dpadMask: 0 },
	{ key: 'S1', action: 13, buttonMask: 256, dpadMask: 0 },
	{ key: 'S2', action: 14, buttonMask: 512, dpadMask: 0 },
	{ key: 'A1', action: 15, buttonMask: 4096, dpadMask: 0 },
	{ key: 'A2', action: 16, buttonMask: 8192, dpadMask: 0 },
	{ key: 'L3', action: 17, buttonMask: 1024, dpadMask: 0 },
	{ key: 'R3', action: 18, buttonMask: 2048, dpadMask: 0 },
	{ key: 'A3', action: 41, buttonMask: 16384, dpadMask: 0 },
	{ key: 'A4', action: 42, buttonMask: 32768, dpadMask: 0 },
	...Array.from({ length: 12 }, (_, index) => ({
		key: `E${index + 1}`, action: 43 + index,
		buttonMask: 2 ** (20 + index), dpadMask: 0,
	})),
] as const;

type OutputMap = Partial<Record<string, string>>;
type ModeDefinition = { value: InputMode; labelKey: string; outputs: OutputMap; axes?: string };
const directions = { Up: 'D-pad Up', Down: 'D-pad Down', Left: 'D-pad Left', Right: 'D-pad Right' };
const xbox = { ...directions, B1: 'A', B2: 'B', B3: 'X', B4: 'Y', L1: 'LB', R1: 'RB', L2: 'LT', R2: 'RT', S1: 'Back', S2: 'Start', A1: 'Guide', L3: 'LS', R3: 'RS' };
const switchButtons = { ...directions, B1: 'B', B2: 'A', B3: 'Y', B4: 'X', L1: 'L', R1: 'R', L2: 'ZL', R2: 'ZR', S1: 'Minus', S2: 'Plus', A1: 'Home', A2: 'Capture', L3: 'L Stick', R3: 'R Stick' };
const playstation = { ...directions, B1: 'Cross', B2: 'Circle', B3: 'Square', B4: 'Triangle', L1: 'L1', R1: 'R1', L2: 'L2', R2: 'R2', S1: 'Select', S2: 'Start', A1: 'PS', A2: 'TP (report bit)', L3: 'L3', R3: 'R3' };
const ps4 = { ...playstation, S1: 'Share', S2: 'Options', A2: 'Touchpad (Center)', A3: 'Touchpad (Left)', A4: 'Touchpad (Right)' };
const miniDirections = { Up: 'Y- (Up)', Down: 'Y+ (Down)', Left: 'X- (Left)', Right: 'X+ (Right)' };
const miniSix = { ...miniDirections, B1: 'A', B2: 'B', B3: 'D', B4: 'E', R1: 'F', R2: 'C', S1: 'Credit', S2: 'Start' };

// 各 src/drivers/*/process と USB descriptor を照合。Buttons.js の表示名は出力の根拠にしない。
export const INPUT_MODE_BUTTON_MAPS: readonly ModeDefinition[] = [
	{ value: InputMode.INPUT_MODE_XINPUT, labelKey: 'xinput', outputs: xbox },
	{ value: InputMode.INPUT_MODE_SWITCH, labelKey: 'nintendo-switch', outputs: switchButtons },
	{ value: InputMode.INPUT_MODE_PS3, labelKey: 'ps3', outputs: playstation },
	{ value: InputMode.INPUT_MODE_KEYBOARD, labelKey: 'keyboard', outputs: {} },
	{ value: InputMode.INPUT_MODE_PS4, labelKey: 'ps4', outputs: ps4 },
	{ value: InputMode.INPUT_MODE_XBONE, labelKey: 'xbone', outputs: { ...xbox, S1: 'View', S2: 'Menu' } },
	{ value: InputMode.INPUT_MODE_MDMINI, labelKey: 'mdmini', outputs: { ...miniDirections, B1: 'A', B2: 'B', B3: 'X', B4: 'Y', R1: 'Z', R2: 'C', S1: 'Mode', S2: 'Start' } },
	{ value: InputMode.INPUT_MODE_NEOGEO, labelKey: 'neogeo', outputs: { ...directions, B1: 'B', B2: 'D', B3: 'A', B4: 'C', L1: 'L1', L2: 'L2', R1: 'R1', R2: 'R2', S1: 'Select', S2: 'Start', A1: 'Options' } },
	{ value: InputMode.INPUT_MODE_PCEMINI, labelKey: 'pcemini', outputs: { ...directions, B1: 'I', B2: 'II', S1: 'Select', S2: 'Run' } },
	{ value: InputMode.INPUT_MODE_EGRET, labelKey: 'egret', outputs: { ...miniSix, A1: 'Menu' } },
	{ value: InputMode.INPUT_MODE_ASTRO, labelKey: 'astro', outputs: miniSix },
	// PSClassicDriver.cpp は S1/S2 を通常の Select/Start と逆に出力。
	{ value: InputMode.INPUT_MODE_PSCLASSIC, labelKey: 'psclassic', outputs: { ...directions, B1: 'Cross', B2: 'Circle', B3: 'Square', B4: 'Triangle', L1: 'L1', R1: 'R1', L2: 'L2', R2: 'R2', S1: 'Start', S2: 'Select' } },
	{ value: InputMode.INPUT_MODE_XBOXORIGINAL, labelKey: 'xboxoriginal', outputs: { ...xbox, L1: 'White', R1: 'Black', A1: undefined } },
	{ value: InputMode.INPUT_MODE_PS5, labelKey: 'ps5', outputs: ps4 },
	{ value: InputMode.INPUT_MODE_GENERIC, labelKey: 'generic', outputs: {
		...Object.fromEntries(MODE_BUTTON_ACTIONS.filter(({ buttonMask }) => buttonMask).map(({ key, buttonMask }) => [key, `Button ${Math.log2(buttonMask) + 1}`])),
		B1: 'Button 2', B2: 'Button 3', B3: 'Button 1',
		Up: 'Hat Up + Button 17', Down: 'Hat Down + Button 18', Left: 'Hat Left + Button 19', Right: 'Hat Right + Button 20',
	} },
	{ value: InputMode.INPUT_MODE_SWITCH_PRO, labelKey: 'nintendo-switch-pro', outputs: switchButtons },
	// XE1AJ::MakeReport は S1/S2 を L2/R2 より前へ移動する。
	{ value: InputMode.INPUT_MODE_XE1AJ, labelKey: 'xe1aj', outputs: {
		...directions, B1: "Button 1 (A/A')", B2: "Button 2 (B/B')", B3: 'Button 3 (C)', B4: 'Button 4 (D)', L1: 'Button 5 (E1)', R1: 'Button 6 (E2)',
		S1: 'Button 7 (Select)', S2: 'Button 8 (Start)', L2: 'Button 9', R2: 'Button 10', L3: 'Button 11', R3: 'Button 12', A1: 'Button 13',
	}, axes: 'LX -> X, LY -> Y, RX -> Z, RY -> Rz' },
];

export function getModeButtonOutput(mode: number, key: string, switchTpShareForDs4: boolean): string | undefined {
	if ((mode === InputMode.INPUT_MODE_PS4 || mode === InputMode.INPUT_MODE_PS5) && switchTpShareForDs4) {
		if (key === 'S1') return 'Touchpad (Center)';
		if (['A2', 'A3', 'A4'].includes(key)) return 'Share';
	}
	return INPUT_MODE_BUTTON_MAPS.find(({ value }) => value === mode)?.outputs[key];
}

export function getPsxSources(profile: Partial<Pick<PinsType, (typeof PSX_INPUTS)[number]['key']>>, row: (typeof MODE_BUTTON_ACTIONS)[number]): string[] {
	return PSX_INPUTS.filter(({ key }) => {
		const payload = profile[key];
		return payload && (payload.action === row.action ||
			(payload.action === BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO &&
				((payload.customButtonMask & row.buttonMask) !== 0 || (payload.customDpadMask & row.dpadMask) !== 0)));
	}).map(({ label }) => label);
}
