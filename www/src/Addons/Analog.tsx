import { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Button, FormCheck, Row, Tab, Tabs } from 'react-bootstrap';
import * as yup from 'yup';

import Section from '../Components/Section';
import FormSelect from '../Components/FormSelect';
import FormControl from '../Components/FormControl';
import { NEGCON_PRESSURE_DEFAULTS, PRESET_ADDON_RULES } from '../Services/PsxPreset';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

const ANALOG_STICK_MODES = [
	{ label: 'Left Analog', value: 1 },
	{ label: 'Right Analog', value: 2 },
];

const INVERT_MODES = [
	{ label: 'None', value: 0 },
	{ label: 'X Axis', value: 1 },
	{ label: 'Y Axis', value: 2 },
	{ label: 'X/Y Axis', value: 3 },
];

const ANALOG_ERROR_RATES = [
	{ label: '0%', value: 1000 },
	{ label: '1%', value: 990 },
	{ label: '2%', value: 979 },
	{ label: '3%', value: 969 },
	{ label: '4%', value: 958 },
	{ label: '5%', value: 946 },
	{ label: '6%', value: 934 },
	{ label: '7%', value: 922 },
	{ label: '8%', value: 911 },
	{ label: '9%', value: 900 },
	{ label: '10%', value: 890 },
	{ label: '11%', value: 876 },
	{ label: '12%', value: 863 },
	{ label: '13%', value: 848 },
	{ label: '14%', value: 834 },
	{ label: '15%', value: 821 },
];

export const NEGCON_MODES = [
	'None', 'LX full', 'LX center+', 'LX center-',
	'LY full', 'LY center+', 'LY center-',
	'RX full', 'RX center+', 'RX center-',
	'RY full', 'RY center+', 'RY center-', 'LT', 'RT',
];

const negconModeSchema = () => yup.number().integer().required().min(0).max(14);

export const analogScheme = {
	...Object.fromEntries(Object.keys(NEGCON_PRESSURE_DEFAULTS).map((key) => {
		const rule = PRESET_ADDON_RULES[key];
		let field = yup.number().integer().required().min(rule.min ?? 0).max(rule.max ?? 1);
		if (rule.choices) field = field.oneOf(rule.choices);
		if (key.endsWith('OuterDeadzone')) field = field.test('pressure-deadzone-order', 'Outer deadzone must exceed inner deadzone.', function (value) {
			return value > Number(this.parent[key.replace('OuterDeadzone', 'InnerDeadzone')]);
		});
		return [key, field];
	})),
	AnalogInputEnabled: yup.number().required().oneOf([1]).label('PSX Analog Input'),
	jogconDeadzone: yup.number().required().min(0).max(32766).test('integer', 'Integer required', Number.isInteger),
	jogconMax: yup.number().required().min(1).max(32767).test('integer', 'Integer required', Number.isInteger)
		.test('jogcon-order', '0 <= deadzone < max <= 32767', function (value) {
			return value > Number(this.parent.jogconDeadzone);
		}),
	jogconForcebackEnabled: yup.number().required().oneOf([0, 1]),
	negconIMode: negconModeSchema(),
	negconIIMode: negconModeSchema(),
	negconLMode: negconModeSchema(),
	negconIThreshold: yup.number().integer().required().min(0).max(255),
	negconIIThreshold: yup.number().integer().required().min(0).max(255),
	negconLThreshold: yup.number().integer().required().min(0).max(255),
	analogAdc1Mode: yup
		.number()
		.label('Analog Stick 1 Mode')
		.required().oneOf(ANALOG_STICK_MODES.map(({ value }) => value)),
	analogAdc1Invert: yup
		.number()
		.label('Analog Stick 1 Invert')
		.required().oneOf(INVERT_MODES.map(({ value }) => value)),
	analogAdc2Mode: yup
		.number()
		.label('Analog Stick 2 Mode')
		.required().oneOf(ANALOG_STICK_MODES.map(({ value }) => value))
		.test('different-stick', 'PSX sticks must use different outputs.', function (mode) {
			return Number(mode) !== Number(this.parent.analogAdc1Mode);
		}),
	analogAdc2Invert: yup
		.number()
		.label('Analog Stick 2 Invert')
		.required().oneOf(INVERT_MODES.map(({ value }) => value)),

	forced_circularity: yup
		.number()
		.label('Force Circularity')
		.required().oneOf([0, 1]),
	forced_circularity2: yup
		.number()
		.label('Force Circularity')
		.required().oneOf([0, 1]),
	inner_deadzone: yup
		.number()
		.label('Inner Deadzone Size (%)')
		.integer().required().min(0).max(100),
	inner_deadzone2: yup
		.number()
		.label('Inner Deadzone Size (%)')
		.integer().required().min(0).max(100),
	outer_deadzone: yup
		.number()
		.label('Outer Deadzone Size (%)')
		.integer().required().min(0).max(100)
		.test('deadzone-order', 'Outer deadzone must exceed inner deadzone.', function (value) {
			return value > Number(this.parent.inner_deadzone);
		}),
	outer_deadzone2: yup
		.number()
		.label('Outer Deadzone Size (%)')
		.integer().required().min(0).max(100)
		.test('deadzone-order', 'Outer deadzone must exceed inner deadzone.', function (value) {
			return value > Number(this.parent.inner_deadzone2);
		}),
	auto_calibrate: yup
		.number()
		.label('Auto Calibration')
		.required().oneOf([0, 1]),
	auto_calibrate2: yup
		.number()
		.label('Auto Calibration')
		.required().oneOf([0, 1]),
	analog_smoothing: yup
		.number()
		.label('Analog Smoothing')
		.required().oneOf([0, 1]),
	analog_smoothing2: yup
		.number()
		.label('Analog Smoothing 2')
		.required().oneOf([0, 1]),
	smoothing_factor: yup
		.number()
		.label('Smoothing Factor')
		.required().min(0).max(100),
	smoothing_factor2: yup
		.number()
		.label('Smoothing Factor 2')
		.required().min(0).max(100),
	analog_error: yup
		.number()
		.label('Error Rate')
		.required().oneOf(ANALOG_ERROR_RATES.map(({ value }) => value)),
	analog_error2: yup
		.number()
		.label('Error Rate 2')
		.required().oneOf(ANALOG_ERROR_RATES.map(({ value }) => value)),
};

export const analogState = {
	...NEGCON_PRESSURE_DEFAULTS,
	AnalogInputEnabled: 1,
	jogconDeadzone: 0,
	jogconMax: 100,
	jogconForcebackEnabled: 1,
	negconIMode: 12,
	negconIIMode: 11,
	negconLMode: 0,
	negconIThreshold: 128,
	negconIIThreshold: 128,
	negconLThreshold: 240,
	analogAdc1PinX: -1,
	analogAdc1PinY: -1,
	analogAdc1Mode: 1,
	analogAdc1Invert: 0,
	analogAdc2PinX: -1,
	analogAdc2PinY: -1,
	analogAdc2Mode: 2,
	analogAdc2Invert: 0,
	forced_circularity: 0,
	forced_circularity2: 0,
	inner_deadzone: 5,
	inner_deadzone2: 5,
	outer_deadzone: 95,
	outer_deadzone2: 95,
	auto_calibrate: 0,
	auto_calibrate2: 0,
	analog_smoothing: 0,
	analog_smoothing2: 0,
	smoothing_factor: 5,
	smoothing_factor2: 5,
	analog_error: 1000,
	analog_error2: 1000,
};

export const ANALOG_PREVIEW_KEYS = Object.keys(analogScheme).filter((key) => key !== 'AnalogInputEnabled');

export function getAnalogPreviewOptions(values: Record<string, unknown>) {
	return Object.fromEntries(ANALOG_PREVIEW_KEYS.map((key) => [key,
		values[key] === '' ? null : Number(values[key]),
	]));
}

const Analog = ({ values, errors, handleChange, handleCheckbox, setFieldValue }: AddonPropTypes) => {
	const { t } = useTranslation();
	const latestValues = useRef(values);
	latestValues.current = values;
	const channel = useRef<BroadcastChannel | null>(null);
	const monitorActive = useRef(false);
	const revision = useRef(0);
	const [previewError, setPreviewError] = useState('');
	const [jogcon, setJogcon] = useState<{ ready?: boolean; resetState?: number } | null>(null);
	const [jogConnected, setJogConnected] = useState(false);
	const [resetError, setResetError] = useState('');
	const [jogError, setJogError] = useState('');
	const [resetPending, setResetPending] = useState(false);
	useEffect(() => {
		let stopped = false;
		let timer: ReturnType<typeof setTimeout>;
		let controller: AbortController | null = null;
		const poll = async () => {
			if (monitorActive.current) { timer = setTimeout(poll, 250); return; }
			controller = new AbortController();
			const timeout = setTimeout(() => controller?.abort(), 2000);
			try {
				const response = await fetch('/api/getAnalogMonitor', { cache: 'no-store', signal: controller.signal });
				const data = await response.json();
				if (!response.ok || data.error) throw new Error(data.error || `HTTP ${response.status}`);
				if (!stopped) {
					setJogError('');
					const connected = data.metadata?.connected === true && data.metadata?.controllerId === 0xe3;
					setJogConnected(connected); setJogcon(connected ? data.jogcon : null);
					if (!connected) setResetError('');
				}
			} catch (issue) { if (!stopped) { setJogConnected(false); setJogcon(null); setJogError(String(issue)); } }
			finally { clearTimeout(timeout); }
			if (!stopped) timer = setTimeout(poll, 250);
		};
		poll();
		return () => { stopped = true; clearTimeout(timer); controller?.abort(); };
	}, []);
	const resetDial = async () => {
		setResetPending(true); setResetError('');
		try {
			const response = await fetch('/api/resetJogconDial', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
			const data = await response.json();
			if (!response.ok || data.error) throw new Error(data.error || `HTTP ${response.status}`);
		} catch (issue) { setResetError(String(issue)); }
		finally { setResetPending(false); }
	};

	useEffect(() => {
		channel.current?.postMessage({ type: 'analog-draft', revision: ++revision.current,
			options: getAnalogPreviewOptions(values) });
	}, [values]);
	useEffect(() => () => channel.current?.close(), []);

	const openMonitor = () => {
		// Device HTTP is not a secure context, where crypto.randomUUID is unavailable.
		const bytes = crypto.getRandomValues(new Uint8Array(16));
		bytes[6] = (bytes[6] & 0x0f) | 0x40;
		bytes[8] = (bytes[8] & 0x3f) | 0x80;
		const hex = Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
		const session = `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
		// Open synchronously inside the click, before any asynchronous API work.
		const monitor = window.open(`/analog-monitor?session=${session}`, '_blank');
		if (!monitor) { setPreviewError('別窓がブロックされました。'); return; }
		monitor.opener = null;
		channel.current?.close();
		channel.current = new BroadcastChannel(`psx-analog-${session}`);
		monitorActive.current = true;
		channel.current.onmessage = (event) => {
			if (event.data?.type === 'monitor-closed') monitorActive.current = false;
			if (event.data?.type === 'monitor-state') {
				const data = event.data.payload;
				const connected = !event.data.error && data?.metadata?.connected === true && data.metadata.controllerId === 0xe3;
				setJogConnected(connected); setJogcon(connected ? data.jogcon : null); setJogError(event.data.error || '');
				if (!connected && !event.data.error) setResetError('');
			}
			if (event.data?.type === 'request-draft') channel.current?.postMessage({
				type: 'analog-draft', revision: ++revision.current,
				options: getAnalogPreviewOptions(latestValues.current),
			});
		};
		setPreviewError('');
	};

	return (
		<Section title={
			<a
				href="https://gp2040-ce.info/add-ons/analog"
				target="_blank"
				className="text-reset text-decoration-none"
			>
				{t('AddonsConfig:analog-header-text')}
			</a>
		}
		>
			<div id="AnalogInputOptions">
				<Button type="button" id="analog-open-monitor" className="mb-3" onClick={openMonitor}>設定値確認</Button>
				{previewError && <p role="alert">{previewError}</p>}
				<div className="alert alert-info" role="alert">
					{t('AddonsConfig:psx-analog-description')}
				</div>
					<Tabs
						defaultActiveKey="analog1Config"
						id="analogConfigTabs"
						className="mb-3 pb-0"
						fill
					>
						<Tab
							key="analog1Config"
							eventKey="analog1Config"
							title={t('AddonsConfig:psx-left-stick')}
						>
							<Row className="mb-3">
								<Row className="mb-3">
									<FormSelect
										label={t('AddonsConfig:psx-stick-output')}
										name="analogAdc1Mode"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc1Mode}
										error={errors.analogAdc1Mode}
										isInvalid={Boolean(errors.analogAdc1Mode)}
										onChange={(event) => {
											const mode = Number(event.target.value);
											setFieldValue('analogAdc1Mode', mode);
											// 出力先は重複できないので、もう一方を反対側へ入れ替える。
											if (mode === Number(values.analogAdc2Mode)) setFieldValue('analogAdc2Mode', mode === 1 ? 2 : 1);
										}}
									>
										{ANALOG_STICK_MODES.map((o, i) => (
											<option key={`button-analogAdc1Mode-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
									<FormSelect
										label={t('AddonsConfig:analog-adc-1-invert-label')}
										name="analogAdc1Invert"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc1Invert}
										error={errors.analogAdc1Invert}
										isInvalid={Boolean(errors.analogAdc1Invert)}
										onChange={handleChange}
									>
										{INVERT_MODES.map((o, i) => (
											<option
												key={`button-analogAdc1Invert-option-${i}`}
												value={o.value}
											>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label={t('AddonsConfig:inner-deadzone-size')}
										name="inner_deadzone"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.inner_deadzone}
										error={errors.inner_deadzone}
										isInvalid={Boolean(errors.inner_deadzone)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
									<FormControl
										type="number"
										label={t('AddonsConfig:outer-deadzone-size')}
										name="outer_deadzone"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.outer_deadzone}
										error={errors.outer_deadzone}
										isInvalid={Boolean(errors.outer_deadzone)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-smoothing')}
										type="switch"
										id="Analog_smoothing"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.analog_smoothing)}
										onChange={(e) => {
											handleCheckbox('analog_smoothing');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.analog_smoothing}
										type="number"
										label={t('AddonsConfig:smoothing-factor')}
										name="smoothing_factor"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.smoothing_factor}
										error={errors.smoothing_factor}
										isInvalid={Boolean(errors.smoothing_factor)}
										onChange={handleChange}
										min={0}
										max={100}
										step="any"
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-force-circularity')}
										type="switch"
										id="Forced_circularity"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.forced_circularity)}
										onChange={(e) => {
											handleCheckbox('forced_circularity');
											handleChange(e);
										}}
									/>
									<FormSelect
										hidden={!values.forced_circularity}
										label={t('AddonsConfig:analog-error-label')}
										name="analog_error"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analog_error}
										onChange={handleChange}
									>
										{ANALOG_ERROR_RATES.map((o, i) => (
											<option key={`analog_error-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<FormCheck
									label={t('AddonsConfig:analog-auto-calibrate')}
									type="switch"
									id="Auto_calibrate"
									className="col-sm-3 ms-3"
									isInvalid={false}
									checked={Boolean(values.auto_calibrate)}
									onChange={(e) => {
										handleCheckbox('auto_calibrate');
										handleChange(e);
									}}
								/>
							</Row>
						</Tab>
						<Tab
							key="analog2Config"
							eventKey="analog2Config"
							title={t('AddonsConfig:psx-right-stick')}
						>
							<Row className="mb-3">
								<Row className="mb-3">
									<FormSelect
										label={t('AddonsConfig:psx-stick-output')}
										name="analogAdc2Mode"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc2Mode}
										error={errors.analogAdc2Mode}
										isInvalid={Boolean(errors.analogAdc2Mode)}
										onChange={(event) => {
											const mode = Number(event.target.value);
											setFieldValue('analogAdc2Mode', mode);
											if (mode === Number(values.analogAdc1Mode)) setFieldValue('analogAdc1Mode', mode === 1 ? 2 : 1);
										}}
									>
										{ANALOG_STICK_MODES.map((o, i) => (
											<option key={`button-analogAdc2Mode-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
									<FormSelect
										label={t('AddonsConfig:analog-adc-2-invert-label')}
										name="analogAdc2Invert"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc2Invert}
										error={errors.analogAdc2Invert}
										isInvalid={Boolean(errors.analogAdc2Invert)}
										onChange={handleChange}
									>
										{INVERT_MODES.map((o, i) => (
											<option
												key={`button-analogAdc2Invert-option-${i}`}
												value={o.value}
											>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label={t('AddonsConfig:inner-deadzone-size')}
										name="inner_deadzone2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.inner_deadzone2}
										error={errors.inner_deadzone2}
										isInvalid={Boolean(errors.inner_deadzone2)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
									<FormControl
										type="number"
										label={t('AddonsConfig:outer-deadzone-size')}
										name="outer_deadzone2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.outer_deadzone2}
										error={errors.outer_deadzone2}
										isInvalid={Boolean(errors.outer_deadzone2)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-smoothing')}
										type="switch"
										id="Analog_smoothing2"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.analog_smoothing2)}
										onChange={(e) => {
											handleCheckbox('analog_smoothing2');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.analog_smoothing2}
										type="number"
										label={t('AddonsConfig:smoothing-factor')}
										name="smoothing_factor2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.smoothing_factor2}
										error={errors.smoothing_factor2}
										isInvalid={Boolean(errors.smoothing_factor2)}
										onChange={handleChange}
										min={0}
										max={100}
										step="any"
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-force-circularity')}
										type="switch"
										id="Forced_circularity2"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.forced_circularity2)}
										onChange={(e) => {
											handleCheckbox('forced_circularity2');
											handleChange(e);
										}}
									/>
									<FormSelect
										hidden={!values.forced_circularity2}
										label={t('AddonsConfig:analog-error-label')}
										name="analog_error2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analog_error2}
										onChange={handleChange}
									>
										{ANALOG_ERROR_RATES.map((o, i) => (
											<option key={`analog_error-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<FormCheck
									label={t('AddonsConfig:analog-auto-calibrate')}
									type="switch"
									id="Auto_calibrate2"
									className="col-sm-3 ms-3"
									isInvalid={false}
									checked={Boolean(values.auto_calibrate2)}
									onChange={(e) => {
										handleCheckbox('auto_calibrate2');
										handleChange(e);
									}}
								/>
							</Row>
						</Tab>
				</Tabs>
				<Section title="JOGCON">
					<Row>
						{(['jogconDeadzone', 'jogconMax'] as const).map((key) => <FormControl key={key} type="number"
							label={`${t(`AddonsConfig:${key === 'jogconDeadzone' ? 'jogcon-deadzone' : 'jogcon-max'}`)} (${t('AddonsConfig:jogcon-counts')})`}
							name={key} groupClassName="col-sm-6 mb-3" value={values[key]} error={errors[key]}
							isInvalid={Boolean(errors[key])} onChange={handleChange} min={key === 'jogconMax' ? 1 : 0} max={32767} step={1} />)}
					</Row>
					<FormCheck type="switch" id="jogcon-forceback" label={t('AddonsConfig:jogcon-forceback')}
						checked={Number(values.jogconForcebackEnabled) === 1}
						onChange={(event) => setFieldValue('jogconForcebackEnabled', event.target.checked ? 1 : 0)} />
					<p id="jogcon-reset-status" role="status">{t(`AddonsConfig:jogcon-state-${jogConnected ? jogcon?.resetState ?? 1 : 0}`)}</p>
					{jogcon?.sleepSuspected && <p role="status">{t('AddonsConfig:jogcon-sleep-help')}</p>}
					<Button id="jogcon-reset-dial" type="button" disabled={!jogConnected || !jogcon?.ready || jogcon?.sleepSuspected || resetPending || jogcon.resetState === 2}
						onClick={resetDial}>{t('AddonsConfig:jogcon-reset')}</Button>
					{resetError && <p role="alert">{resetError}</p>}
					{jogError && <p role="alert">{jogError}</p>}
				</Section>
				<Section title="neGcon I / II / L">
				{(['I', 'II', 'L'] as const).map((input) => {
					const mode = `negcon${input}Mode` as 'negconIMode' | 'negconIIMode' | 'negconLMode';
					const threshold = `negcon${input}Threshold` as 'negconIThreshold' | 'negconIIThreshold' | 'negconLThreshold';
					const digitalEnabled = `negcon${input}DigitalEnabled` as const;
					return <Row key={input} className="mb-3">
						<FormSelect label={`${input} ${t('AddonsConfig:negcon-output')}`} name={mode}
							groupClassName="col-sm-6" value={values[mode]} error={errors[mode]}
							isInvalid={Boolean(errors[mode])} onChange={handleChange}>
							{NEGCON_MODES.map((label, value) => <option key={value} value={value}>{label}</option>)}
						</FormSelect>
						<FormControl type="number" label={`${input} ${t('AddonsConfig:negcon-threshold')}`}
							name={threshold} groupClassName="col-sm-6" value={values[threshold]}
							error={errors[threshold]} isInvalid={Boolean(errors[threshold])}
							onChange={handleChange} min={0} max={255} step={1} disabled={Number(values[digitalEnabled]) !== 1} />
						{(['InnerDeadzone', 'OuterDeadzone'] as const).map((suffix) => {
								const key = `negcon${input}${suffix}` as const;
							return <FormControl key={key} type="number" name={key}
								label={`${input} ${t(`AddonsConfig:negcon-${suffix === 'InnerDeadzone' ? 'inner' : 'outer'}-deadzone`)}`}
								groupClassName="col-sm-6" value={values[key]} error={errors[key]}
								isInvalid={Boolean(errors[key])} onChange={handleChange} min={0} max={100} step={1} />;
						})}
						<FormCheck type="switch" id={digitalEnabled} name={digitalEnabled} className="ms-3"
							label={`${input} ${t('AddonsConfig:negcon-digital-enabled')}`}
							checked={Number(values[digitalEnabled]) === 1} isInvalid={Boolean(errors[digitalEnabled])}
							onChange={(event) => setFieldValue(digitalEnabled, event.target.checked ? 1 : 0)} />
					</Row>;
				})}
				</Section>
			</div>
		</Section>
	);
};

export default Analog;
