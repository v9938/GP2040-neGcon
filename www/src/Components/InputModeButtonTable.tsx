import { useContext, useEffect, useState } from 'react';
import { Alert, Button, Form, Spinner, Table } from 'react-bootstrap';
import { useTranslation } from 'react-i18next';
import { AppContext } from '../Contexts/AppContext';
import { getButtonLabels } from '../Data/Buttons';
import { KEY_CODES } from '../Data/Keyboard';
import { getModeButtonOutput, getPsxSources, INPUT_MODE_BUTTON_MAPS, MODE_BUTTON_ACTIONS } from '../Data/InputModeButtonMap';
import type { PinsType } from '../Store/useProfilesStore';

type TableSettings = { inputMode: number; switchTpShareForDs4: number; inputDeviceType: number };

export default function InputModeButtonTable({ profile, profileIndex }: { profile: PinsType; profileIndex: number }) {
	const { t } = useTranslation();
	const { buttonLabels } = useContext(AppContext);
	const labels: Record<string, string> = getButtonLabels(buttonLabels.buttonLabelType, buttonLabels.swapTpShareLabels);
	const [open, setOpen] = useState(false);
	const [revision, setRevision] = useState(0);
	const [settings, setSettings] = useState<TableSettings | null>(null);
	const [keyMappings, setKeyMappings] = useState<Record<string, number>>({});
	const [mode, setMode] = useState<number | null>(null);
	const [loading, setLoading] = useState(false);
	const [error, setError] = useState(false);

	useEffect(() => {
		if (!open) return;
		let alive = true;
		const controller = new AbortController();
		const timeout = window.setTimeout(() => controller.abort(), 10000);
		setLoading(true);
		setError(false);
		// 比較表の読込は同一 origin の GET のみ。モード選択・再読込で設定を書き換えない。
		async function load() {
			try {
				const read = async (endpoint: string) => {
					const response = await fetch(`/api/${endpoint}`, { signal: controller.signal });
					if (!response.ok) throw new Error(endpoint);
					return response.json();
				};
				const [options, keys] = await Promise.all([read('getGamepadOptions'), read('getKeyMappings')]);
				if (!options || !Number.isInteger(options.inputMode) || !INPUT_MODE_BUTTON_MAPS.some(({ value }) => value === options.inputMode) || !keys || typeof keys !== 'object') throw new Error('Invalid settings');
				if (alive) {
					setSettings(options);
					setKeyMappings(keys);
					setMode(options.inputMode);
				}
			} catch {
				if (alive) setError(true);
			} finally {
				window.clearTimeout(timeout);
				if (alive) setLoading(false);
			}
		}
		load();
		return () => { alive = false; window.clearTimeout(timeout); controller.abort(); };
	}, [open, revision]);

	const definition = INPUT_MODE_BUTTON_MAPS.find(({ value }) => value === mode);
	const labelType = mode === 1 || mode === 15 ? 'switch' : mode === 13 ? 'ps4' : mode === 8 ? 'pcengine' : definition?.labelKey;
	const id = `mode-button-table-${profileIndex}`;
	return (
		<details className="mb-3" data-testid={id} onToggle={(event) => setOpen(event.currentTarget.open)}>
			<summary>{t('PinMapping:mode-table-title')}</summary>
			{open && <div className="mt-3">
				<p>{t('PinMapping:mode-table-description')}</p>
				<Button type="button" size="sm" variant="secondary" disabled={loading} onClick={() => setRevision((value) => value + 1)} data-testid={`${id}-reload`}>{t('PinMapping:mode-table-reload')}</Button>
				{loading ? <div role="status" className="my-3"><Spinner size="sm" animation="border" /> {t('PinMapping:mode-table-loading')}</div> : error ? <Alert variant="danger" className="mt-3">{t('PinMapping:mode-table-error')}</Alert> : settings && definition && <>
					<Form.Group className="my-3" controlId={`${id}-mode`}>
						<Form.Label>{t('PinMapping:mode-table-select')}</Form.Label>
						<Form.Select value={mode ?? ''} onChange={(event) => setMode(Number(event.target.value))} data-testid={`${id}-mode`}>
							{INPUT_MODE_BUTTON_MAPS.map(({ value, labelKey }) => <option key={value} value={value}>{value}: {t(`SettingsPage:input-mode-options.${labelKey}`)}</option>)}
						</Form.Select>
						<Form.Text>{t('PinMapping:mode-table-local-only')} {t('PinMapping:mode-table-current', { mode: `${settings.inputMode}: ${t(`SettingsPage:input-mode-options.${INPUT_MODE_BUTTON_MAPS.find(({ value }) => value === settings.inputMode)?.labelKey}`)}` })}</Form.Text>
					</Form.Group>
					{buttonLabels.buttonLabelType !== labelType && <Alert variant="info">{t('PinMapping:mode-table-label-warning', { label: labels.label })}</Alert>}
					{[0, 2, 4, 13].includes(mode!) && <p className="small">{t('PinMapping:mode-table-variant', { deviceType: settings.inputDeviceType })}</p>}
					<Table responsive bordered size="sm" data-testid={`${id}-rows`}>
						<thead><tr><th>{t('PinMapping:mode-table-output')}</th><th>{t('PinMapping:mode-table-action')}</th><th>{t('PinMapping:mode-table-page-label')}</th><th>{t('PinMapping:mode-table-sources')}</th></tr></thead>
						<tbody>{MODE_BUTTON_ACTIONS.map((row) => {
							const key = keyMappings[row.key];
							const output = mode === 3 ? (Number.isInteger(key) && key > 0 ? `${KEY_CODES.find(({ value }) => value === key)?.label ?? 'Key'} (HID ${key})` : key === 0 ? undefined : t('PinMapping:mode-table-key-unavailable')) : getModeButtonOutput(mode!, row.key, settings.switchTpShareForDs4 === 1);
							const sources = getPsxSources(profile, row);
							return <tr key={row.action} data-action={row.key} data-testid={`${id}-action-${row.action}`} className={output ? undefined : 'table-secondary'}>
								<td title={output}>{output || t('PinMapping:mode-table-unsupported')}</td>
								<td>{row.key} <small>({row.action})</small></td><td>{labels[row.key] || row.key}</td>
								<td>{sources.length ? sources.join(', ') : t('PinMapping:mode-table-unassigned')}</td>
							</tr>;
						})}</tbody>
					</Table>
					{definition.axes && <p className="small">{t('PinMapping:mode-table-axes')}: {definition.axes}</p>}
				</>}
			</div>}
		</details>
	);
}
