import type { PsxPreset } from './PsxPreset';
import { validatePreset } from './PsxPreset';

export const PSX_PRESET_STAGED_EVENT = 'psx-preset-staged';
let pendingSharedPreset: PsxPreset | null = null;
let pendingProfileIndex: number | null = null;
let addonDraft: Record<string, unknown> | null = null;
let reactiveDraft: PsxPreset['reactiveLEDs'] | null = null;
let macroDraft: PsxPreset['macros'] | null = null;
let pendingCategories = { addons: false, reactiveLEDs: false, macros: false };
const copy = <T>(value: T): T => structuredClone(value);

// ブラウザーのRAMだけに保持し、読込時には機器APIを呼ばない。
export function stagePresetShared(preset: PsxPreset, profileIndex?: number) {
	validatePreset(preset);
	pendingSharedPreset = copy(preset);
	pendingProfileIndex = profileIndex ?? null;
	pendingCategories = { addons: true, reactiveLEDs: true, macros: true };
	addonDraft = copy(preset.addons);
	const keyboard = addonDraft.keyboardHostMap as Record<string, number>;
	addonDraft.keyboardHostMap = Object.fromEntries(Object.entries(keyboard).map(([key, value]) => [key, { key: value, pin: -1, error: null }]));
	reactiveDraft = copy(preset.reactiveLEDs);
	macroDraft = copy(preset.macros);
	if (typeof window !== 'undefined') window.dispatchEvent(new CustomEvent(PSX_PRESET_STAGED_EVENT));
}
export const peekPendingPresetShared = () => copy(pendingSharedPreset);
export const getPendingPresetProfileIndex = () => pendingProfileIndex;
export const getPendingPresetCategories = () => ({ ...pendingCategories });
export function consumePendingPresetShared() { const preset = peekPendingPresetShared(); clearPendingPresetShared(); return preset; }
export function clearPendingPresetShared() { pendingSharedPreset = null; pendingProfileIndex = null; pendingCategories = { addons: false, reactiveLEDs: false, macros: false }; }
function clearCategory(category: keyof typeof pendingCategories) {
	pendingCategories[category] = false;
	if (!Object.values(pendingCategories).some(Boolean)) { pendingSharedPreset = null; pendingProfileIndex = null; }
}
export const getAddonDraft = () => copy(addonDraft);
export const setAddonDraft = (value: Record<string, unknown> | null) => { addonDraft = copy(value); if (value === null) clearCategory('addons'); };
export const getReactiveDraft = () => copy(reactiveDraft);
export const setReactiveDraft = (value: PsxPreset['reactiveLEDs'] | null) => { reactiveDraft = copy(value); if (value === null) clearCategory('reactiveLEDs'); };
export const getMacroDraft = () => copy(macroDraft);
export const setMacroDraft = (value: PsxPreset['macros'] | null) => { macroDraft = copy(value); if (value === null) clearCategory('macros'); };
