import { useContext, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import * as yup from 'yup';
import isEqual from 'lodash/isEqual';

import { AppContext } from '../Contexts/AppContext';
import Section from '../Components/Section';
import FormSelect from '../Components/FormSelect';
import FormControl from '../Components/FormControl';
import { Button, FormCheck } from 'react-bootstrap';
import WebApi, { formatReactiveLEDs } from '../Services/WebApi';
import { getReactiveDraft, setReactiveDraft, PSX_PRESET_STAGED_EVENT } from '../Services/PsxDraft';
import { BUTTON_ACTIONS } from '../Data/Pins';
import { getButtonLabels } from '../Data/Buttons';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

import './ReactiveLED.scss';

const CHANNELS = ['NEOPIXEL[R]', 'NEOPIXEL[G]', 'NEOPIXEL[B]'];
const ACTIONS = Object.entries(BUTTON_ACTIONS).filter(([, value]) =>
	value === BUTTON_ACTIONS.NONE || (value >= 1 && value <= 18) || (value >= 59 && value <= 66),
);

export const REACTIVE_LED_MODES = [
	{ label: 'static-off', value: 0 },
	{ label: 'static-on', value: 1 },
	{ label: 'fade-in', value: 2 },
	{ label: 'fade-out', value: 3 },
	{ label: 'analog-proportional', value: 4 },
];

export const reactiveLEDScheme = {
	ReactiveLEDAddonEnabled: yup.number().integer().oneOf([0, 1]).required(),
};

export const reactiveLEDState = {
	ReactiveLEDAddonEnabled: 1,
};

export const reactiveLEDSettingsSchema = yup.object({
	leds: yup.array().length(3).of(yup.object({
			action: yup.number().integer().oneOf(ACTIONS.map(([, value]) => value)).required(),
			modeUp: yup.number().integer().oneOf(REACTIVE_LED_MODES.map(({ value }) => value)).required(),
			modeDown: yup.number().integer().oneOf(REACTIVE_LED_MODES.map(({ value }) => value)).required(),
			brightness: yup.number().integer().min(0).max(255).required(),
		}).required()).required(),
});

type LEDSettings = {
	action: number;
	modeUp: number;
	modeDown: number;
	brightness: number;
};

const ReactiveLED = ({ values, handleCheckbox }: AddonPropTypes) => {
	const { t } = useTranslation();
	const { setLoading, buttonLabels } = useContext(AppContext);
	const [reactiveLEDs, setReactiveLEDs] = useState<LEDSettings[]>(getReactiveDraft()?.leds || formatReactiveLEDs().leds);
	const [storedLEDs, setStoredLEDs] = useState<LEDSettings[] | null>(null);
	const [saveMessage, setSaveMessage] = useState('');
	const [saving, setSaving] = useState(false);
	const [errors, setErrors] = useState<Record<string, string>>({});
	const currentButtons = getButtonLabels(buttonLabels.buttonLabelType, buttonLabels.swapTpShareLabels);

	useEffect(() => {
		let mounted = true;
		const applyDraft = () => {
			const draft = getReactiveDraft();
			if (draft) { setReactiveLEDs(draft.leds); setErrors({}); setSaveMessage(''); }
		};
		window.addEventListener(PSX_PRESET_STAGED_EVENT, applyDraft);
		async function fetchData() {
			const settings = await WebApi.getReactiveLEDs(setLoading);
			if (!mounted) return;
			if (settings) {
				setReactiveLEDs(getReactiveDraft()?.leds || settings.leds);
				setStoredLEDs(structuredClone(settings.leds));
			} else {
				setSaveMessage(t('ReactiveLED:load-error'));
			}
		}
		fetchData().catch(() => { if (mounted) setSaveMessage(t('ReactiveLED:load-error')); });
		const reload = () => fetchData().catch(console.error);
		window.addEventListener('psx-preset-applied', reload);
		return () => { mounted = false; window.removeEventListener(PSX_PRESET_STAGED_EVENT, applyDraft); window.removeEventListener('psx-preset-applied', reload); };
	}, [setLoading, t]);

	const handleLEDChange = (index: number, field: keyof LEDSettings, value: number) => {
		const next = reactiveLEDs.map((led, i) => i === index ? { ...led, [field]: value } : led);
		setReactiveLEDs(next);
		setReactiveDraft({ leds: next });
		setSaveMessage('');
		setErrors({});
	};

	const saveLEDSettings = async () => {
		setErrors({});
		// API が HTTP 200 で error を返す場合も、成功時だけ baseline を更新する。
		const candidate = structuredClone({ leds: reactiveLEDs });
		try {
			await reactiveLEDSettingsSchema.validate(candidate, { strict: true, abortEarly: false });
		} catch (error) {
			if (error instanceof yup.ValidationError) {
				setErrors(Object.fromEntries(error.inner.map((issue) => [
					(issue.path || '').replace(/\[(\d+)\]/g, '.$1'), issue.message,
				])));
			}
			setSaveMessage(t('Common:saved-error-message'));
			return;
		}
		if (storedLEDs && isEqual(candidate.leds, storedLEDs)) {
			setReactiveDraft(null);
			setSaveMessage(t('Common:saved-success-message'));
			return;
		}
		setSaving(true);
		let success = false;
		try {
			success = Boolean(await WebApi.setReactiveLEDs(candidate));
		} catch (error) {
			console.error(error);
		}
		if (success) {
			setStoredLEDs(candidate.leds);
			if (isEqual(getReactiveDraft()?.leds, candidate.leds)) setReactiveDraft(null);
		}
		setSaveMessage(t(success ? 'Common:saved-success-message' : 'Common:saved-error-message'));
		setSaving(false);
	};

	return (
		<Section title={t('ReactiveLED:header-text')}>
			<p>{t('ReactiveLED:fixed-wiring')}</p>
			<p>LED設定は全プロファイル共通です。読込だけでは保存されません。</p>
			<div id="ReactiveLEDEnabledOptions" hidden={!values.ReactiveLEDAddonEnabled}>
				<div className="reactive-led-grid gap-3">
					{CHANNELS.map((channel, index) => (
						<div key={channel}>
							<h5>{channel}</h5>
							<FormSelect
								label={t('ReactiveLED:led-action-label')}
								aria-label={`${channel} ${t('ReactiveLED:led-action-label')}`}
								name={`led${index}Action`}
								className="form-select-sm"
								groupClassName="mb-3"
								value={reactiveLEDs[index].action}
								error={errors[`leds.${index}.action`]}
								isInvalid={Boolean(errors[`leds.${index}.action`])}
								onChange={(e) => handleLEDChange(index, 'action', Number(e.target.value))}
							>
								{ACTIONS.map(([key, value]) => (
									<option key={key} value={value}>
										{key.startsWith('ANALOG_DIRECTION_')
											? t(`PinMapping:actions.${key}`)
											: currentButtons[key.replace('BUTTON_PRESS_', '')] || t(`PinMapping:actions.${key}`)}
									</option>
								))}
							</FormSelect>
							{(['modeUp', 'modeDown'] as const).map((field) => {
								const label = t(`ReactiveLED:led-mode-${field === 'modeUp' ? 'up' : 'down'}-label`);
								return (
									<FormSelect
										key={field}
										label={label}
										aria-label={`${channel} ${label}`}
										name={`led${index}${field === 'modeUp' ? 'ModeUp' : 'ModeDown'}`}
										className="form-select-sm"
										groupClassName="mb-3"
										value={reactiveLEDs[index][field]}
										error={errors[`leds.${index}.${field}`]}
										isInvalid={Boolean(errors[`leds.${index}.${field}`])}
										onChange={(e) => handleLEDChange(index, field, Number(e.target.value))}
									>
										{REACTIVE_LED_MODES.map(({ label, value }) => (
											<option key={value} value={value}>{t(`ReactiveLED:led-mode.${label}`)}</option>
										))}
									</FormSelect>
								);
							})}
							<FormControl
								type="number"
								label={t('ReactiveLED:led-brightness-label')}
								aria-label={`${channel} ${t('ReactiveLED:led-brightness-label')}`}
								name={`led${index}Brightness`}
								className="form-control-sm"
								groupClassName="mb-3"
								value={Number.isNaN(reactiveLEDs[index].brightness) ? '' : reactiveLEDs[index].brightness}
								error={errors[`leds.${index}.brightness`]}
								isInvalid={Boolean(errors[`leds.${index}.brightness`])}
								onChange={(e) => handleLEDChange(index, 'brightness', (e.target as HTMLInputElement).valueAsNumber)}
								min={0}
								max={255}
								step={1}
							/>
						</div>
					))}
				</div>
				<Button type="button" id="saveReactiveLEDs" className="mb-3" disabled={saving} onClick={saveLEDSettings}>
					{t('ReactiveLED:button-save')}
				</Button>
			</div>
			{saveMessage && <p role="status">{saveMessage}</p>}
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="ReactiveLEDAddonButton"
				reverse
				checked={Boolean(values.ReactiveLEDAddonEnabled)}
				onChange={() => handleCheckbox('ReactiveLEDAddonEnabled')}
			/>
		</Section>
	);
};

export default ReactiveLED;
