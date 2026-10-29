import { ChangeEvent, useEffect, useRef, useState } from 'react';
import { Alert, Button, Form, Modal } from 'react-bootstrap';
import { PSX_INPUTS } from '../Data/PSX';
import WebApi from '../Services/WebApi';
import useProfilesStore from '../Store/useProfilesStore';
import { getAddonDraft, getMacroDraft, getReactiveDraft, getPendingPresetCategories, getPendingPresetProfileIndex, peekPendingPresetShared, setAddonDraft, setMacroDraft, setReactiveDraft, stagePresetShared, PSX_PRESET_STAGED_EVENT } from '../Services/PsxDraft';
import { parsePreset, PRESET_ADDON_RULES, PRESET_KEYBOARD_BUTTONS, PRESET_MAX_BYTES, PSX_PRESET_INPUTS, PsxPreset, serializePreset, validatePreset } from '../Services/PsxPreset';

export default function PsxPresetFiles({ profileIndex, activeProfileIndex }: { profileIndex: number; activeProfileIndex?: number }) {
	const fileInput = useRef<HTMLInputElement>(null);
	const [candidate, setCandidate] = useState<PsxPreset | null>(null);
	const [importedPreset, setImportedPreset] = useState<PsxPreset | null>(() => getPendingPresetProfileIndex() === profileIndex ? peekPendingPresetShared() : null);
	const [includeShared, setIncludeShared] = useState(true);
	const [busy, setBusy] = useState(false);
	const [message, setMessage] = useState('');
	const [error, setError] = useState(false);
	const report = (text: string, failed = false) => { setMessage(text); setError(failed); };
	useEffect(() => {
		const refresh = () => { if (getPendingPresetProfileIndex() === profileIndex) setImportedPreset(peekPendingPresetShared()); };
		window.addEventListener(PSX_PRESET_STAGED_EVENT, refresh);
		return () => window.removeEventListener(PSX_PRESET_STAGED_EVENT, refresh);
	}, [profileIndex]);

	const makePreset = (profile: any, addons: any, reactiveLEDs: any, macros: any): PsxPreset => ({
		profile: { profileLabel: profile.profileLabel || `Profile ${profileIndex + 1}`, enabled: Boolean(profile.enabled), source: profileIndex + 1 },
		mapping: Object.fromEntries(PSX_INPUTS.map(({ key }, i) => [PSX_PRESET_INPUTS[i], profile[key]])),
		addons: {
			...Object.fromEntries(Object.keys(PRESET_ADDON_RULES).map((key) => [key, addons[key]])),
			keyboardHostMap: Object.fromEntries(PRESET_KEYBOARD_BUTTONS.map((key) => {
				const entry = addons.keyboardHostMap[key];
				return [key, typeof entry === 'object' && entry !== null ? entry.key : entry];
			})),
		},
		reactiveLEDs: { leds: reactiveLEDs.leds }, macros: { macroList: macros.macroList },
	});

	const saveImported = async () => {
		if (!importedPreset) return;
		setBusy(true);
		let writing = false;
		try {
			const store = useProfilesStore.getState();
			const profiles = structuredClone(store.profiles);
			const categories = getPendingPresetProfileIndex() === profileIndex
				? getPendingPresetCategories() : { addons: false, macros: false, reactiveLEDs: false };
			const addonSnapshot = getAddonDraft();
			const macroSnapshot = getMacroDraft();
			const reactiveSnapshot = getReactiveDraft();
			const preset = validatePreset(makePreset(profiles[profileIndex],
				addonSnapshot ?? importedPreset.addons,
				reactiveSnapshot ?? importedPreset.reactiveLEDs,
				macroSnapshot ?? importedPreset.macros));
			if ((profileIndex === 0 || profileIndex === activeProfileIndex) && !preset.profile.enabled) throw new Error('基本または使用中のプロファイルは無効にできません。');
			const checkResponse = (response: any) => {
				const body = response?.data ?? response;
				if (!body || body.error || body.success === false) throw new Error('機器が保存を拒否しました。');
			};
			writing = true;
			// APIカテゴリを順次保存する。一部成功後の失敗をロールバックとは表示しない。
			if (categories.addons) checkResponse(await WebApi.setAddonsOptions(preset.addons));
			if (categories.macros) checkResponse(await WebApi.setMacroAddonOptions(preset.macros));
			if (categories.reactiveLEDs) checkResponse(await WebApi.setReactiveLEDs(preset.reactiveLEDs));
			if (profileIndex === 0) checkResponse(await WebApi.setPinMappings(profiles[0]));
			else checkResponse(await WebApi.setProfileOptions(profiles.slice(1)));
			// 保存中に入力された新しい下書きを破棄しない。
			if (categories.addons && JSON.stringify(getAddonDraft()) === JSON.stringify(addonSnapshot)) setAddonDraft(null);
			if (categories.macros && JSON.stringify(getMacroDraft()) === JSON.stringify(macroSnapshot)) setMacroDraft(null);
			if (categories.reactiveLEDs && JSON.stringify(getReactiveDraft()) === JSON.stringify(reactiveSnapshot)) setReactiveDraft(null);
			setImportedPreset(null);
			window.dispatchEvent(new CustomEvent('psx-preset-applied'));
			report('読込設定を機器へ保存しました。自動再起動は行っていません。');
		} catch (cause) { report(`${cause instanceof Error ? cause.message : '保存に失敗しました。'}${writing ? ' 一部の設定は保存済みの可能性があります。下書きを保持しています。内容を確認して再度保存してください。' : ' 機器への保存は開始していません。'}`, true); }
		finally { setBusy(false); }
	};

	const exportFile = async () => {
		setBusy(true);
		try {
			// 非同期GET中に編集されても、開始時の選択プロファイルを出力する。
			const profile = structuredClone(useProfilesStore.getState().profiles[profileIndex]);
			const [addons, reactiveLEDs, macros] = await Promise.all([
				getAddonDraft() ?? WebApi.getAddonsOptions(),
				getReactiveDraft() ?? WebApi.getReactiveLEDs(),
				getMacroDraft() ?? WebApi.getMacroAddonOptions(),
			]);
			const preset = makePreset(profile, addons, reactiveLEDs, macros);
			const url = URL.createObjectURL(new Blob([serializePreset(preset)], { type: 'text/plain;charset=utf-8' }));
			const link = document.createElement('a');
			link.href = url;
			link.download = `${preset.profile.profileLabel.replace(/[^a-zA-Z0-9_-]/g, '_') || 'Profile'}.gp2040-psx.txt`;
			link.click();
			setTimeout(() => URL.revokeObjectURL(url), 1000);
			report('TXTをPCへ出力しました。プロファイルは編集中の値、共有設定は下書きがあればその値、なければ機器から取得した値です。機器への保存は行っていません。');
		} catch (cause) { report(cause instanceof Error ? cause.message : 'TXTの出力に失敗しました。', true); }
		finally { setBusy(false); }
	};

	const readFile = async (event: ChangeEvent<HTMLInputElement>) => {
		const file = event.target.files?.[0];
		event.target.value = '';
		if (!file) return;
		setBusy(true);
		try {
			if (!/\.gp2040-psx\.txt$/i.test(file.name)) throw new Error('拡張子は .gp2040-psx.txt を使用してください。');
			if (file.size > PRESET_MAX_BYTES) throw new Error('TXTは64KiB以下にしてください。');
			const text = new TextDecoder('utf-8', { fatal: true }).decode(await file.arrayBuffer());
			setCandidate(parsePreset(text));
			setIncludeShared(true);
			report('');
		} catch (cause) { report(cause instanceof Error ? cause.message : 'TXTの読込に失敗しました。', true); }
		finally { setBusy(false); }
	};

	const importFile = () => {
		if (!candidate) return;
		try {
			if ((profileIndex === 0 || profileIndex === activeProfileIndex) && !candidate.profile.enabled) throw new Error('基本または使用中のプロファイルを無効にするTXTは読み込めません。');
			useProfilesStore.getState().replacePresetProfile(profileIndex, candidate);
			if (includeShared) stagePresetShared(candidate, profileIndex);
			setImportedPreset(includeShared ? candidate : null);
			setCandidate(null);
			report(includeShared
				? '選択プロファイルと共有設定を下書きへ読み込みました。機器には未保存です。「読込設定を保存」でまとめて保存できます。共有設定は全プロファイルに影響します。'
				: '選択プロファイルだけを下書きへ読み込みました。共有設定は変更していません。この画面の保存ボタンで機器へ保存してください。');
		} catch (cause) { report(cause instanceof Error ? cause.message : 'TXTの反映に失敗しました。', true); }
	};

	return (
		<div className="my-3">
			<div className="d-flex flex-wrap gap-2">
				<Button type="button" variant="outline-primary" disabled={busy} onClick={exportFile}>PCへ保存（TXT）</Button>
				<Button type="button" variant="outline-primary" disabled={busy} onClick={() => fileInput.current?.click()}>PCから読込（TXT）</Button>
				{importedPreset && <Button type="button" variant="warning" disabled={busy} onClick={saveImported}>読込設定を保存</Button>}
				<input ref={fileInput} aria-label="PSXプリセットTXTファイル" type="file" accept=".gp2040-psx.txt" hidden onChange={readFile} />
			</div>
			<p className="text-muted mt-2">選択プロファイルのPSX 16入力と、全プロファイル共通の8種類のADDON設定をTXTで保存します。読込先は選択中のプロファイルです。USB出力モード・固定配線・認証情報は含みません。</p>
			{message && <Alert role="status" variant={error ? 'danger' : 'info'}>{message}</Alert>}
			<Modal show={Boolean(candidate)} onHide={() => setCandidate(null)}>
				<Modal.Header closeButton><Modal.Title>TXT読込の確認</Modal.Title></Modal.Header>
				<Modal.Body>
					<p>「{candidate?.profile.profileLabel}」（出力元 {candidate?.profile.source}）の16入力を選択中のプロファイル {profileIndex + 1} の下書きへ上書きします。他のプロファイルと現在の使用プロファイルは切り替えません。</p>
					<Form.Check id={`preset-shared-${profileIndex}`} type="checkbox" checked={includeShared} onChange={(event) => setIncludeShared(event.target.checked)} label="共有ADDON・Keyboard Host・RGB Reactive LED・6件のMacroも下書きへ読み込む" />
					<Alert variant="warning" className="mt-3">共有設定は全プロファイルに影響します。現在の未保存の共有下書きも置き換えます。確認しても機器への保存は行いません。</Alert>
				</Modal.Body>
				<Modal.Footer>
					<Button type="button" variant="secondary" onClick={() => setCandidate(null)}>キャンセル</Button>
					<Button type="button" onClick={importFile}>下書きへ読み込む</Button>
				</Modal.Footer>
			</Modal>
		</div>
	);
}
