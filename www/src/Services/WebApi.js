import Http from './Http';

export const baseUrl = process.env.NODE_ENV === 'production'
	? '' : (import.meta.env.VITE_DEV_BASE_URL || '');

export const baseButtonMappings = Object.fromEntries([
	'Up', 'Down', 'Left', 'Right', 'B1', 'B2', 'B3', 'B4', 'L1', 'R1',
	'L2', 'R2', 'S1', 'S2', 'L3', 'R3', 'A1', 'A2', 'A3', 'A4',
	'E1', 'E2', 'E3', 'E4', 'E5', 'E6', 'E7', 'E8', 'E9', 'E10', 'E11', 'E12', 'Fn',
].map((button) => [button, { pin: -1, key: 0, error: null }]));

export const basePeripheralMapping = {
	peripheral: { usb0: { enabled: 1, dp: 6, enable5v: -1, order: 0 } },
};

async function getOptions(endpoint, setLoading = () => {}) {
	setLoading(true);
	try {
		return (await Http.get(`${baseUrl}/api/${endpoint}`)).data;
	} finally {
		setLoading(false);
	}
}

async function setOptions(endpoint, options) {
	const request = { ...options };
	delete request.usedPins;
	return (await Http.post(`${baseUrl}/api/${endpoint}`, request)).data;
}

const resetSettings = () => getOptions('resetSettings');
const getGamepadOptions = (loading) => getOptions('getGamepadOptions', loading);
const setGamepadOptions = (options) => setOptions('setGamepadOptions', options);
const getPinMappings = () => getOptions('getPinMappings');
const setPinMappings = (options) => Http.post(`${baseUrl}/api/setPinMappings`, options);
const getProfileOptions = async () => (await getOptions('getProfileOptions')).alternativePinMappings;
const setProfileOptions = (mappings) => Http.post(`${baseUrl}/api/setProfileOptions`, {
	alternativePinMappings: mappings,
});

async function getKeyMappings(loading) {
	const data = await getOptions('getKeyMappings', loading);
	const mappings = structuredClone(baseButtonMappings);
	for (const [button, key] of Object.entries(data)) {
		mappings[button] = { ...(mappings[button] || {}), key: Number(key) };
	}
	return mappings;
}

const setKeyMappings = (mappings) => setOptions('setKeyMappings',
	Object.fromEntries(Object.entries(mappings).map(([button, value]) =>
		[button, typeof value === 'object' ? value.key : value])),
);

async function getAddonsOptions(loading) {
	const data = await getOptions('getAddonsOptions', loading);
	const keyboardHostMap = structuredClone(baseButtonMappings);
	for (const [button, key] of Object.entries(data.keyboardHostMap || {})) {
		keyboardHostMap[button] = { ...(keyboardHostMap[button] || {}), key: Number(key) };
	}
	return { ...data, keyboardHostMap };
}

async function setAddonsOptions(options) {
	const request = structuredClone(options);
	if (request.keyboardHostMap) {
		request.keyboardHostMap = Object.fromEntries(Object.entries(request.keyboardHostMap)
			.map(([button, value]) => [button, typeof value === 'object' ? value.key : value]));
	}
	return setOptions('setAddonsOptions', request);
}

const getMacroAddonOptions = (loading) => getOptions('getMacroAddonOptions', loading);
const setMacroAddonOptions = (options) => setOptions('setMacroAddonOptions', options);
const setPS4Options = (options) => setOptions('setPS4Options', options);

export function formatReactiveLEDs(data = {}) {
	// 先頭の RGB 3 チャンネルだけを扱い、API の数値キーを維持する。
	return {
		leds: [59, 60, 66].map((action, index) => {
			const led = data.leds?.[index] || {};
			return {
				action: led.action ?? action,
				modeUp: led.modeUp ?? 0,
				modeDown: led.modeDown ?? 4,
				brightness: led.brightness ?? 255,
			};
		}),
	};
}

const getReactiveLEDs = async (loading) => formatReactiveLEDs(await getOptions('getReactiveLEDs', loading));
const setReactiveLEDs = (options) => setOptions('setReactiveLEDs', formatReactiveLEDs(options));
const getPeripheralOptions = (loading) => getOptions('getPeripheralOptions', loading);
const getUsedPins = (loading) => getOptions('getUsedPins', loading);

async function getHeldPins(signal) {
	try {
		return (await Http.get(`${baseUrl}/api/getHeldPins`, { signal })).data;
	} catch (error) {
		if (error.name === 'AbortError') return { canceled: true };
		throw error;
	}
}

async function abortGetHeldPins() {
	try {
		await getOptions('abortGetHeldPins');
	} catch {
		// キャプチャ中断時の切断は許容する。
	}
}

const reboot = (bootMode) => setOptions('reboot', { bootMode });

export default {
	resetSettings, getGamepadOptions, setGamepadOptions,
	getPinMappings, setPinMappings, getProfileOptions, setProfileOptions,
	getKeyMappings, setKeyMappings, getAddonsOptions, setAddonsOptions,
	getMacroAddonOptions, setMacroAddonOptions, setPS4Options,
	getReactiveLEDs, setReactiveLEDs, getPeripheralOptions, getUsedPins,
	getHeldPins, abortGetHeldPins, reboot,
};
