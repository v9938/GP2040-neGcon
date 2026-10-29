import { useEffect, useRef, useState } from 'react';
import { Button, Modal } from 'react-bootstrap';
import { useTranslation } from 'react-i18next';
import WebApi from '../Services/WebApi';
import { PSX_INPUTS } from '../Data/PSX';

type CaptureButtonTypes = {
	labels: string[];
	onChange: (label: string, pin: number) => void;
	small?: boolean;
	buttonLabel?: string;
};

const CaptureButton = ({
	labels,
	onChange,
	small = false,
	buttonLabel,
}: CaptureButtonTypes) => {
	const { t } = useTranslation('');
	const controller = useRef<null | AbortController>(null);
	const stopRef = useRef(false);
	const [showModal, setShowModal] = useState(false);
	const [labelIndex, setLabelIndex] = useState(0);
	const [triggerCapture, setTriggerCapture] = useState(false);
	const [captureError, setCaptureError] = useState('');
	const [simulation, setSimulation] = useState(false);
	const lastEvent = useRef<number | null>(null);
	const generation = useRef<number | null>(null);
	const lastCapturedPin = useRef<number | null>(null);

	useEffect(() => () => {
		stopRef.current = true;
		controller.current?.abort();
	}, []);

	const currentLabel = labels[labelIndex] || '';
	const hasNext = Boolean(labels[labelIndex + 1]);

	const timeout = (ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms));

	const closeAndReset = () => {
		stopRef.current = false;
		setShowModal(false);
		setLabelIndex(0);
		lastEvent.current = null;
		generation.current = null;
		lastCapturedPin.current = null;
	};

	const getHeldPins = async () => {
		setTriggerCapture(false);
		controller.current = new AbortController();
		const signal = controller.current.signal;
		let released = labelIndex === 0;
		// API は即時 snapshot。各ラベルで一度離してから新しい押下を待つ。
		while (!signal.aborted) {
			let data;
			try {
				const request = new AbortController();
				const abort = () => request.abort();
				signal.addEventListener('abort', abort, { once: true });
				const timer = setTimeout(abort, 2000);
				try { data = await WebApi.getHeldPins(request.signal); }
				finally { clearTimeout(timer); signal.removeEventListener('abort', abort); }
			} catch {
				setCaptureError('PSX input request failed. Check the controller connection and restart capture.');
				if (signal.aborted) break;
				await timeout(100);
				continue;
			}
			if (signal.aborted) break;
			if (!data || data.canceled) { await timeout(100); continue; }
			setSimulation(data.simulated === true);
			let pin = data.heldPins?.find((slot: number) => PSX_INPUTS.some((input) => input.slot === slot));
			if (Number.isInteger(data.buttonEventSequence)) {
				if (generation.current !== data.generation) {
					generation.current = data.generation;
					lastEvent.current = data.buttonEventSequence;
				} else if (lastEvent.current !== data.buttonEventSequence) {
					lastEvent.current = data.buttonEventSequence;
					pin = data.pressedPins?.find((slot: number) => PSX_INPUTS.some((input) => input.slot === slot));
					released = true;
				}
			}
			setCaptureError(data.connected === false ? 'PSX controller not connected.' : '');
			if (pin === undefined) released = true;
			// 直前と異なるボタンなら、HTTP取得間の短い解放を見逃しても受け付ける。
			// 同じボタンの連続割当は解放または新規イベントを必須とする。
			else if (released || (lastCapturedPin.current !== null && pin !== lastCapturedPin.current)) {
				lastCapturedPin.current = pin;
				onChange(currentLabel, pin);
				break;
			}
			await timeout(100);
		}

		if (stopRef.current || !hasNext) return closeAndReset();

		setLabelIndex((index) => index + 1);
		setTriggerCapture(true);
	};

	const stopCapture = () => {
		stopRef.current = true;
		controller?.current?.abort();
	};

	const skipButton = () => {
		controller?.current?.abort();
	};

	useEffect(() => {
		if (triggerCapture) {
			setShowModal(true);
			getHeldPins();
		}
	}, [triggerCapture]);

	return (
		<>
			<Modal centered show={showModal} onHide={() => stopCapture()}>
				<Modal.Header closeButton>
					<Modal.Title className="me-auto">{`${t(
						'CaptureButton:capture-button-modal-title',
					)} ${currentLabel}`}</Modal.Title>
				</Modal.Header>
				<Modal.Body className="row">
					{simulation && <div className="mb-3">
						<p>PC模擬入力：割り当てたいPSXボタンをクリックしてください。</p>
						<div className="d-flex flex-wrap gap-2">{PSX_INPUTS.map(({ slot, label }) =>
							<Button key={slot} type="button" variant="outline-primary" onClick={async () => {
								try {
									const send = async (heldPins: number[]) => {
										const response = await fetch('/api/previewSimulate', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ heldPins }) });
										if (!response.ok) throw new Error('模擬入力に失敗しました。');
									};
									await send([]); await timeout(150); await send([slot]);
									await timeout(300); await send([]);
								} catch { setCaptureError('模擬入力に失敗しました。'); }
							}}> {label} </Button>)}</div>
					</div>}
					{captureError && <div role="alert" className="alert alert-warning">{captureError}</div>}
					<span className="col-sm-10">
						{t('CaptureButton:capture-button-modal-content')}
					</span>
					<span className="col-sm-1">
						<span className="spinner-border" />
					</span>
				</Modal.Body>
				<Modal.Footer>
					{hasNext && (
						<Button onClick={() => skipButton()}>
							{t('CaptureButton:capture-button-modal-skip')}
						</Button>
					)}
					<Button variant="danger" onClick={() => stopCapture()}>
						{t('CaptureButton:capture-button-modal-stop')}
					</Button>
				</Modal.Footer>
			</Modal>
			<Button data-psx-capture="true" onClick={() => setTriggerCapture(true)}>
				{small
					? '🎮'
					: `${
							buttonLabel
								? buttonLabel
								: t('CaptureButton:capture-button-button-label')
						} 🎮`}
			</Button>
		</>
	);
};

export default CaptureButton;
