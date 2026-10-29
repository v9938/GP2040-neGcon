import { useEffect, useRef, useState } from 'react';
import { Alert, Button, Col, Form, Row } from 'react-bootstrap';
import { useTranslation } from 'react-i18next';
import Section from '../Components/Section';
import './AnalogMonitor.scss';

type Axes = { lx?: number; ly?: number; rx?: number; ry?: number; lt?: number; rt?: number };
type Raw = Axes & { i?: number; ii?: number; l?: number; jogPosition?: number; jogStatus?: number };
type SimulationKey = 'lx' | 'ly' | 'rx' | 'ry' | 'i' | 'ii' | 'l' | 'jogPosition' | 'jogStatus';
type MonitorPayload = {
	raw?: Raw; savedComputed?: Axes | null; draftComputed?: Axes | null;
	metadata?: Record<string, unknown> & { connected?: boolean; controllerId?: number; ageMs?: number; draftErrors?: string[] };
	simulation?: boolean; draftErrors?: string[]; error?: string;
	jogcon?: { position?: number; status?: number; motorPower?: number; motorCommand?: number; ready?: boolean; resetState?: number };
};
const COLORS = { raw: '#adb5bd', saved: '#49b8ec' };
const validSession = (value: string) => /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(value);
const numberInRange = (value: unknown, max: number): value is number =>
	typeof value === 'number' && Number.isFinite(value) && value >= 0 && value <= max;

function StickPlot({ name, x, y, payload, available }: {
	name: string; x: 'lx' | 'rx'; y: 'ly' | 'ry'; payload: MonitorPayload | null; available: boolean;
}) {
	const points = [
		{ label: 'RAW', axes: payload?.raw, max: 255, color: COLORS.raw },
		{ label: '計算値', axes: payload?.savedComputed, max: 65535, color: COLORS.saved },
	];
	const jog = payload?.metadata?.controllerId === 0xe3;
	const jogAxis = Number(payload?.metadata?.jogconOutputMode) === 2 ? 'rx' : 'lx';
	const jogPosition = payload?.raw?.jogPosition;
	return <Section title={name}>
		<svg className="analog-monitor-plot" viewBox="0 0 256 256" role="img" aria-label={`${name} XY`}>
			<rect x="1" y="1" width="254" height="254" fill="none" stroke="currentColor" />
			<path d="M128 1V255 M1 128H255" stroke="currentColor" opacity="0.35" />
			{available && points.map(({ label, axes, max, color }) => {
				if (label === 'RAW' && jog) return x === jogAxis && typeof jogPosition === 'number' && Number.isFinite(jogPosition)
					? <circle key={label} data-kind="jogcon-raw" cx={128 + Math.max(-1, Math.min(1, jogPosition / 32767)) * 126} cy={128} r="5" fill={color} /> : null;
				const vx = axes?.[x]; const vy = axes?.[y];
				// neGconのねじり等の一軸入力は中央線に描画し、欠けた軸の数値はN/Aのままにする。
				const validX = numberInRange(vx, max); const validY = numberInRange(vy, max);
				return validX || validY
					? <circle key={label} cx={validX ? 2 + vx / max * 252 : 128} cy={validY ? 2 + vy / max * 252 : 128} r="5" fill={color} /> : null;
			})}
		</svg>
		{points.map(({ label, axes, max, color }) => <p key={label} style={{ color }}>
			{label === 'RAW' && jog ? `RAW: ${available && x === jogAxis ? jogPosition ?? 'N/A' : 'N/A'} counts` : <>{label}: {x.toUpperCase()} {available && numberInRange(axes?.[x], max) ? axes?.[x] : 'N/A'} / {y.toUpperCase()} {available && numberInRange(axes?.[y], max) ? axes?.[y] : 'N/A'}</>}
		</p>)}
	</Section>;
}

export default function AnalogMonitor() {
	const { t } = useTranslation();
	const [session] = useState(() => new URLSearchParams(window.location.search).get('session') || '');
	const [payload, setPayload] = useState<MonitorPayload | null>(null);
	const [error, setError] = useState('');
	const [age, setAge] = useState(Infinity);
	const [resetError, setResetError] = useState('');
	const [resetPending, setResetPending] = useState(false);
	const pendingInput = useRef<Record<string, number> | null>(null);
	const pendingReset = useRef(false);
	const simulated = useRef(false);
	const statusChannel = useRef<BroadcastChannel | null>(null);
	const [simulationInput, setSimulationInput] = useState({ controllerId: 0x79, lx: 128, ly: 128, rx: 128, ry: 128, i: 0, ii: 0, l: 0, jogPosition: 0, jogStatus: 0 });
	useEffect(() => {
		if (!validSession(session)) return;
		const channel = new BroadcastChannel(`psx-analog-${session}`);
		statusChannel.current = channel;
		const close = () => channel.postMessage({ type: 'monitor-closed' });
		window.addEventListener('pagehide', close);
		return () => { close(); channel.close(); statusChannel.current = null; window.removeEventListener('pagehide', close); };
	}, [session]);
	useEffect(() => { statusChannel.current?.postMessage({ type: 'monitor-state', payload, error }); }, [payload, error]);

	useEffect(() => {
		if (!validSession(session)) { setError('設定画面の「設定値確認」から開いてください。'); return; }
		let stopped = false;
		let timer: ReturnType<typeof setTimeout>;
		let controller: AbortController | null = null;
		let lastResponse = 0;
		let lastAge = Infinity;
		let consecutiveFailures = 0;
		const ageTimer = setInterval(() => {
			if (!stopped) setAge(lastAge + (lastResponse ? performance.now() - lastResponse : 0));
		}, 200);
		async function request(path: string, body?: Record<string, unknown>) {
			const requestController = new AbortController();
			controller = requestController;
			const timeout = setTimeout(() => requestController.abort(), 4000);
			try {
				const response = await fetch(path, { signal: requestController.signal, cache: 'no-store', headers: { 'X-Analog-Session': session },
					...(body ? { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) } : {}) });
				const data = await response.json();
				if (!response.ok || data.error) throw new Error(data.error || `HTTP ${response.status}`);
				return data;
			} finally { clearTimeout(timeout); }
		}
		async function poll() {
			const start = performance.now();
			try {
				if (stopped) return;
				if (pendingInput.current) {
					const input = pendingInput.current; pendingInput.current = null;
					await request('/api/setAnalogSimulationInput', { session, ...input });
				}
				if (pendingReset.current) {
					pendingReset.current = false;
					try { await request('/api/resetJogconDial', simulated.current ? { session } : {}); }
					catch (issue) { if (!stopped) setResetError(String(issue)); }
					finally { if (!stopped) setResetPending(false); }
				}
				if (stopped) return;
				const data: MonitorPayload = await request('/api/getAnalogMonitor');
				if (stopped) return;
				lastResponse = performance.now();
				lastAge = Number(data.metadata?.ageMs ?? Infinity);
				setPayload(data); setAge(lastAge); setError('');
				consecutiveFailures = 0;
				simulated.current = data.simulation === true;
				if (data.metadata?.connected !== true) setResetError('');
				if (!pendingInput.current && data.simulation && data.metadata?.controllerId === 0xe3 && typeof data.raw?.jogPosition === 'number') {
					setSimulationInput((input) => ({ ...input, jogPosition: data.raw!.jogPosition! }));
				}
			} catch (issue) {
				consecutiveFailures = Math.min(consecutiveFailures + 1, 4);
				if (!stopped) setError(issue instanceof Error && issue.name === 'AbortError'
					? '通信応答がタイムアウトしました。自動再接続を待っています。' : String(issue));
			}
			if (!stopped) {
				setAge(lastAge + (lastResponse ? performance.now() - lastResponse : 0));
				// 表示は5Hzで十分。裏の窓は1Hz、失敗中は待機を延ばして接続負荷を抑える。
				const interval = consecutiveFailures ? Math.min(4000, 500 * 2 ** consecutiveFailures)
					: document.hidden ? 1000 : 200;
				timer = setTimeout(poll, Math.max(consecutiveFailures ? interval : 50,
					interval - (performance.now() - start)));
			}
		}
		poll();
		const close = () => { stopped = true; clearTimeout(timer); clearInterval(ageTimer); controller?.abort(); };
		window.addEventListener('pagehide', close);
		return () => { close(); window.removeEventListener('pagehide', close); };
	}, [session]);

	const id = Number(payload?.metadata?.controllerId);
	const connected = payload?.metadata?.connected === true;
	const analog = connected && [0x53, 0x73, 0x79].includes(id);
	const negcon = connected && id === 0x23;
	const jog = connected && id === 0xe3;
	const resetDial = () => {
		setResetPending(true); setResetError('');
		pendingReset.current = true;
	};
	const changeSimulation = (key: string, value: number) => {
		const min = key === 'jogPosition' ? -32768 : 0;
		const max = key === 'jogPosition' ? 32767 : 255;
		if (!Number.isInteger(value) || value < min || value > max) return;
		const next = { ...simulationInput, [key]: value };
		setSimulationInput(next); pendingInput.current = next;
	};
	return <main className="analog-monitor" id="analog-monitor">
		<div className="d-flex justify-content-between align-items-center gap-2"><h1>設定値確認</h1>
			<Button type="button" variant="secondary" onClick={() => { window.close(); if (!window.closed) window.location.assign('/add-ons'); }}>閉じる</Button></div>
		<p>RAW：PSXの入力値　計算値：現在保存されている設定を適用した値</p>
		{error && <Alert variant="danger" id="analog-monitor-error">{error}</Alert>}
		{(!connected || age > 1000) && <Alert variant="warning" id="analog-monitor-stale">{connected ? '入力が古くなっています' : '未接続 / 実測値 N/A'} ({Number.isFinite(age) ? Math.round(age) : 'N/A'} ms)</Alert>}
		<Row><Col md={6}><StickPlot name="Left XY" x="lx" y="ly" payload={payload} available={analog || negcon || jog} /></Col>
			<Col md={6}><StickPlot name="Right XY" x="rx" y="ry" payload={payload} available={analog || negcon || jog} /></Col></Row>
		{jog && <Section title="JOGCON">
			<p id="jogcon-raw-position">{t('AddonsConfig:jogcon-position')}: {payload?.raw?.jogPosition ?? 'N/A'} {t('AddonsConfig:jogcon-counts')}</p>
			<p id="jogcon-motor">{t('AddonsConfig:jogcon-feedback')}: {payload?.jogcon?.motorPower ?? 'N/A'} / 15, 0x{(payload?.jogcon?.motorCommand ?? 0).toString(16).padStart(2, '0')} | RAW status: {payload?.raw?.jogStatus ?? 'N/A'}</p>
            {payload?.jogcon?.sleepSuspected && <Alert variant="warning">{t('AddonsConfig:jogcon-sleep-help')}</Alert>}
            <p id="jogcon-reset-status" role="status" data-reset-state={payload?.jogcon?.resetState ?? 0}>{t(`AddonsConfig:jogcon-state-${payload?.jogcon?.resetState ?? 0}`)}</p>
			<Button type="button" id="jogcon-reset-dial" disabled={!payload?.jogcon?.ready || payload?.jogcon?.sleepSuspected || resetPending || payload.jogcon.resetState === 2 || age > 1000 || Boolean(error)} onClick={resetDial}>{t('AddonsConfig:jogcon-reset')}</Button>
			{resetError && <p role="alert">{resetError}</p>}
		</Section>}
		{negcon && <Section title="neGcon RAW (0..255)">{(['lx', 'i', 'ii', 'l'] as const).map((key) => <p key={key}>{key === 'lx' ? 'Twist' : key.toUpperCase()}: {payload?.raw?.[key] ?? 'N/A'}</p>)}</Section>}
		<Section title="出力値">
			{(['lx', 'ly', 'rx', 'ry', 'lt', 'rt'] as const).map((key) => <div key={key} className="mb-2">
				<strong>{key === 'lt' ? 'ZL (LT)' : key === 'rt' ? 'ZR (RT)' : key.toUpperCase()}</strong>
				{(['savedComputed'] as const).map((kind) => {
					const value = payload?.[kind]?.[key]; const max = key === 'lt' || key === 'rt' ? 255 : 65535;
					const valid = (analog || negcon || jog) && numberInRange(value, max);
					return <div key={kind} style={{ color: COLORS.saved }}>
						計算値: {valid ? value : 'N/A'} / {max}
						<progress className="w-100" value={valid ? value : 0} max={max} aria-label={`${kind} ${key}`} />
					</div>;
				})}</div>)}
		</Section>
		{payload?.simulation === true && <Section title="PC 模擬入力">
			<Form.Select id="analog-simulation-id" aria-label="Controller ID" value={simulationInput.controllerId} onChange={(event) => changeSimulation('controllerId', Number(event.target.value))}>
				<option value={0x41}>Digital (0x41)</option><option value={0x79}>DS2 (0x79)</option><option value={0x23}>neGcon (0x23)</option>
				<option value={0xe3}>JOGCON (0xE3)</option>
			</Form.Select>
			{simulationInput.controllerId === 0xe3 && <>
				<Form.Label htmlFor="analog-simulation-jogPosition">{t('AddonsConfig:jogcon-position')} ({t('AddonsConfig:jogcon-counts')})</Form.Label>
				<Form.Range id="analog-simulation-jogPosition" min={-300} max={300} step={1} value={Math.max(-300, Math.min(300, simulationInput.jogPosition))} onChange={(event) => changeSimulation('jogPosition', Number(event.target.value))} />
				<Form.Control id="analog-simulation-jogPosition-number" type="number" min={-32768} max={32767} step={1} value={simulationInput.jogPosition} onChange={(event) => changeSimulation('jogPosition', Number(event.target.value))} />
				<Form.Label htmlFor="analog-simulation-jogStatus">RAW status (0..255)</Form.Label>
				<Form.Control id="analog-simulation-jogStatus" type="number" min={0} max={255} step={1} value={simulationInput.jogStatus} onChange={(event) => changeSimulation('jogStatus', Number(event.target.value))} />
			</>}
			{((simulationInput.controllerId === 0x23 ? ['lx', 'i', 'ii', 'l'] : simulationInput.controllerId === 0x79 ? ['lx', 'ly', 'rx', 'ry'] : []) as SimulationKey[]).map((key) => <Form.Group key={key}>
				<Form.Label htmlFor={`analog-simulation-${key}`}>{key === 'lx' && simulationInput.controllerId === 0x23 ? 'Twist' : key.toUpperCase()}: {simulationInput[key]}</Form.Label>
				<Form.Range id={`analog-simulation-${key}`} min={0} max={255} step={1} value={simulationInput[key]} onChange={(event) => changeSimulation(key, Number(event.target.value))} />
			</Form.Group>)}
		</Section>}
		<details><summary>Metadata</summary><pre>{JSON.stringify(payload?.metadata || {}, null, 2)}</pre></details>
	</main>;
}
