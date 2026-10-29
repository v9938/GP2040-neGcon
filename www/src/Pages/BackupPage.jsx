import { useEffect, useState, useRef, useContext } from 'react';
import { AppContext } from '../Contexts/AppContext';
import { Button, Form, Col } from 'react-bootstrap';
import { useTranslation } from 'react-i18next';
import { DEFAULT_VALUES } from './AddonsConfigPage';

import Section from '../Components/Section';
import WebApi from '../Services/WebApi';

const FILE_EXTENSION = '.gp2040';
const FILENAME = 'gp2040ce_backup_{DATE}' + FILE_EXTENSION;

const API_BINDING = {
	gamepad: {
		get: WebApi.getGamepadOptions,
		set: WebApi.setGamepadOptions,
	},
	macros: {
		get: WebApi.getMacroAddonOptions,
		set: WebApi.setMacroAddonOptions,
	},
	pins: {
		get: WebApi.getPinMappings,
		set: WebApi.setPinMappings,
	},
	profiles: {
		get: WebApi.getProfileOptions,
		set: WebApi.setProfileOptions,
	},
	addons: {
		get: WebApi.getAddonsOptions,
		set: WebApi.setAddonsOptions,
	},
	reactiveLEDs: { get: WebApi.getReactiveLEDs, set: WebApi.setReactiveLEDs },
	keyMappings: { get: WebApi.getKeyMappings, set: WebApi.setKeyMappings },
};

export default function BackupPage() {
	const inputFileSelect = useRef();

	const [optionState, setOptionStateData] = useState({});
	const [importOptions, setImportOptions] = useState({});
	const [exportOptions, setExportOptions] = useState({});

	const [noticeMessage, setNoticeMessage] = useState('');
	const [saveMessage, setSaveMessage] = useState('');
	const [loadMessage, setLoadMessage] = useState('');
	const { setLoading } = useContext(AppContext);

	const { t } = useTranslation('');

	useEffect(() => {
		async function fetchData() {
			let exportData = {};
			for (const [key, func] of Object.entries(API_BINDING)) {
				exportData[key] = await func.get(setLoading);
			}
			setOptionStateData(exportData);
		}
		fetchData().catch((error) => setNoticeMessage(error.message));

		let defaults = {};
		for (const [key] of Object.entries(API_BINDING)) {
			defaults[key] = true;
		}
		setImportOptions(defaults);
		setExportOptions(defaults);
	}, []);

	const validateValues = (data, nextData) => {
		// 同一バージョンのカテゴリを丸ごと復元し、配列や未割当を保持する。
		return structuredClone(nextData);
	};

	const setOptionsToAPIStorage = async (options) => {
		for (const [key, func] of Object.entries(API_BINDING)) {
			const values = options[key];
			if (values !== undefined) {
				const result = await func.set(values);
				if (!result || result.error !== undefined || result.success === false)
					throw new Error(`Failed to set ${key} options`);
				setOptionStateData((previous) => ({ ...previous, [key]: values }));
			}
		}
	};

	const handleSave = async () => {
		let exportData = {};
		for (const [key, value] of Object.entries(exportOptions)) {
			if (value) {
				try {
					const current = await API_BINDING[key].get(setLoading);
					exportData[key] = key === 'addons'
						? Object.fromEntries(Object.entries(current).filter(([name]) => name in DEFAULT_VALUES))
						: key === 'macros' ? { macroList: current.macroList } : current;
				} catch (error) {
					setNoticeMessage(error.message);
					return;
				}
			}
		}

		const fileDate = new Date().toISOString().replace(/[^0-9]/g, '');
		const name = FILENAME.replace('{DATE}', fileDate);
		const json = JSON.stringify(exportData);
		const file = new Blob([json], { type: 'text/json;charset=utf-8' });

		let a = document.createElement('a');
		a.href = URL.createObjectURL(file);
		a.download = name;
		a.innerHTML = 'Save Backup';

		let container = document.getElementById('root');
		container.appendChild(a);

		a.click();
		a.remove();
		URL.revokeObjectURL(a.href);

		setSaveMessage(t('BackupPage:saved-success-message', { name }));

		setTimeout(() => {
			setSaveMessage('');
		}, 5000);
	};

	const handleFileSelect = (ev) => {
		const input = ev.target;
		if (!input) {
			setNoticeMessage(`Unknown browser error, missing event data!`);
			return;
		}
		if (input.files.length === 0) {
			setNoticeMessage(`No files are loaded.`);
			return;
		}

		const fileName = input.files[0].name;

		let reader = new FileReader();
		reader.onload = async function () {
			setLoadMessage('');
			let fileData = undefined;
			try {
				fileData = JSON.parse(reader.result);
			} catch (e) {
				// error dialog
				setNoticeMessage(`Failed to parse data for ${fileName}!`);
				return;
			}
			if (!fileData || typeof fileData !== 'object' || Array.isArray(fileData)) {
				setNoticeMessage(`No file data found for ${fileName}`);
				return;
			}

			let filteredData = {};
			const ignored = Object.keys(fileData).filter((key) => !(key in API_BINDING));
			for (const [key, value] of Object.entries(importOptions)) {
				if (value && fileData[key] !== undefined) {
					let validData = validateValues(optionState[key], fileData[key]);
					if (key === 'addons') {
						const legacy = Object.keys(validData).filter((name) => !(name in DEFAULT_VALUES));
						ignored.push(...legacy.map((name) => `addons.${name}`));
						validData = Object.fromEntries(Object.entries(validData).filter(([name]) => name in DEFAULT_VALUES));
					}
					if (key === 'macros') validData = { macroList: validData.macroList };
					filteredData[key] = validData;
				}
			}

			if (Object.keys(filteredData).length > 0) {
				try {
					await setOptionsToAPIStorage(filteredData);
					setLoadMessage(`Loaded ${fileName}`);
					setNoticeMessage(ignored.length ? t('BackupPage:ignored-legacy', { categories: ignored.join(', ') }) : '');
				} catch (error) {
					setNoticeMessage(error.message);
					return;
				}

				setTimeout(() => {
					setLoadMessage('');
				}, 5000);
			} else {
				setNoticeMessage(t('BackupPage:ignored-legacy', { categories: ignored.join(', ') }));
			}
		};
		reader.onerror = () => {
			setNoticeMessage(`Error occured while reading ${fileName}.`);
		};
		reader.readAsText(input.files[0]);
	};

	return (
		<>
			<Section title={t('BackupPage:save-header-text')}>
				<Col>
					<Form.Group className={'row mb-3'}>
						<div className={'col'}>
							{Object.entries(API_BINDING).map(([key]) => (
								<Form.Check
									id={`export_${key}`}
									key={`export_${key}`}
									label={t('BackupPage:save-export-option-label', {
										api: t(`BackupPage:api-${key}-text`),
									})}
									type={'checkbox'}
									checked={exportOptions[key] ?? false}
									onChange={() => {
										setExportOptions((prev) => ({
											...prev,
											[key]: !prev[key],
										}));
									}}
								/>
							))}
						</div>
					</Form.Group>
					<div
						style={{
							display: 'flex',
							flexDirection: 'row',
						}}
					>
						<Button type="submit" onClick={handleSave}>
							{t('Common:button-save-label')}
						</Button>
						<div
							style={{
								height: '100%',
								paddingLeft: 24,
								fontWeight: 600,
								color: 'darkcyan',
								alignSelf: 'center',
							}}
						>
							{saveMessage ? saveMessage : null}
						</div>
					</div>
				</Col>
			</Section>
			<Section title={t('BackupPage:load-header-text')}>
				<div className="alert alert-warning">
					{t(`BackupPage:pin-version-warning-text`)}
				</div>
				<Col>
					<Form.Group className={'row mb-3'}>
						<div className={'col'}>
							{Object.entries(API_BINDING).map(([key]) => (
								<Form.Check
									id={`import_${key}`}
									key={`import_${key}`}
									label={t('BackupPage:load-export-option-label', {
										api: t(`BackupPage:api-${key}-text`),
									})}
									type={'checkbox'}
									checked={importOptions[key] ?? false}
									onChange={() => {
										setImportOptions((prev) => ({
											...prev,
											[key]: !prev[key],
										}));
									}}
								/>
							))}
						</div>
					</Form.Group>
					<input
						ref={inputFileSelect}
						type={'file'}
						accept={FILE_EXTENSION}
						style={{ display: 'none' }}
						onChange={handleFileSelect.bind(this)}
					/>
					<div
						style={{
							display: 'flex',
							flexDirection: 'row',
						}}
					>
						<Button
							onClick={() => {
								inputFileSelect.current.click();
							}}
						>
							{t('Common:button-load-label')}
						</Button>
						<div
							style={{
								height: '100%',
								paddingLeft: 24,
								fontWeight: 600,
								color: 'darkcyan',
								alignSelf: 'center',
							}}
						>
							<span>{loadMessage ? loadMessage : null}</span>
							<span style={{ color: 'red', fontWeight: 'bold' }}>
								{noticeMessage ? noticeMessage : null}
							</span>
						</div>
					</div>
				</Col>
			</Section>
		</>
	);
}
