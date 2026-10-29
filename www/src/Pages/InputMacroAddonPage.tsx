import { useContext, useEffect, useState } from 'react';
import {
	Badge,
	Button,
	Col,
	Form,
	InputGroup,
	Nav,
	Row,
	Tab,
	Tabs,
	Table,
	Alert,
} from 'react-bootstrap';

import { Formik, useFormikContext } from 'formik';
import * as yup from 'yup';
import { Trans, useTranslation } from 'react-i18next';
import omit from 'lodash/omit';
import isEqual from 'lodash/isEqual';

import { AppContext } from '../Contexts/AppContext';
import Section from '../Components/Section';
import WebApi from '../Services/WebApi';
import { getMacroDraft, setMacroDraft, PSX_PRESET_STAGED_EVENT } from '../Services/PsxDraft';
import {
	getButtonLabels,
	BUTTONS,
	BUTTON_MASKS_OPTIONS,
} from '../Data/Buttons';

const MACRO_TYPES = [
	{ label: 'InputMacroAddon:input-macro-type.press', value: 1 },
	{ label: 'InputMacroAddon:input-macro-type.hold-repeat', value: 2 },
	{ label: 'InputMacroAddon:input-macro-type.toggle', value: 3 },
];
const MACRO_INPUTS_MAX = 30;
const MACRO_LIMIT = 6;

const schema = yup.object().shape({
	macroList: yup.array().of(
		yup.object().shape({
			macroType: yup.number().required().oneOf([1, 2, 3]),
			macroLabel: yup.string().test('utf8-length', 'Macro label must be at most 64 UTF-8 bytes.',
				(value) => new TextEncoder().encode(value || '').length <= 64),
			enabled: yup.number().required().oneOf([0, 1]),
			exclusive: yup.number(),
			interruptible: yup.number(),
			showFrames: yup.number(),
			useMacroTriggerButton: yup.number(),
			macroTriggerButton: yup.number(),
			macroInputs: yup
				.array()
				.max(MACRO_INPUTS_MAX, 'Exceeded maximum inputs')
				.of(
					yup.object().shape({
						buttonMask: yup.number().integer().min(0).required(),
						duration: yup.number().integer().min(0).max(4294967295).required(),
						waitDuration: yup.number().integer().min(0).max(4294967295).required(),
					}),
				),
		}),
	).required().length(MACRO_LIMIT),
});

const defaultMacroInput = {
	buttonMask: 0,
	duration: 16666,
	waitDuration: 0,
};

export const defaultValues = {
	macroList: Array.from({ length: MACRO_LIMIT }, () => ({
		macroType: 1,
		macroLabel: '',
		enabled: 0,
		exclusive: 1,
		interruptible: 1,
		showFrames: 1,
		useMacroTriggerButton: 0,
		macroTriggerButton: 0,
		macroInputs: [{ ...defaultMacroInput }],
	})),
};

const ONE_FRAME_US = 16666;

const FormContext = () => {
	const { setValues, values } = useFormikContext<typeof defaultValues>();
	const { setLoading } = useContext(AppContext);
	const [loaded, setLoaded] = useState(false);

	useEffect(() => {
		let mounted = true;
		const applyDraft = () => { const draft = getMacroDraft(); if (draft) setValues(draft); };
		window.addEventListener(PSX_PRESET_STAGED_EVENT, applyDraft);
		async function fetchData() {
			const options = await WebApi.getMacroAddonOptions(setLoading);
			if (!mounted) return;
			setValues(getMacroDraft() || { macroList: options?.macroList || defaultValues.macroList });
			setLoaded(true);
		}
		fetchData().catch(console.error);
		const reload = () => fetchData().catch(console.error);
		window.addEventListener('psx-preset-applied', reload);
		return () => { mounted = false; window.removeEventListener(PSX_PRESET_STAGED_EVENT, applyDraft); window.removeEventListener('psx-preset-applied', reload); };
	}, [setValues]);
	useEffect(() => { if (loaded) setMacroDraft(values); }, [loaded, values]);

	return null;
};

const ButtonMasksComponent = (props) => {
	const {
		id: key,
		value,
		onChange,
		isInvalid,
		buttonLabelType,
		buttonMasks,
	} = props;
	return (
		<Form.Select
			size="sm"
			id={key}
			name={`${key}.buttonMask`}
			value={value}
			isInvalid={isInvalid}
			onChange={onChange}
		>
			{buttonMasks.map((o, i2) => (
				<option key={`${key}.mask[${i2}]`} value={o.value}>
					{(buttonLabelType && BUTTONS[buttonLabelType][o.label]) || o.label}
				</option>
			))}
		</Form.Select>
	);
};

const MacroInputComponent = (props) => {
	const {
		value: { duration, buttonMask, waitDuration },
		buttonLabelType,
		showFrames,
		errors,
		id: key,
		translation: t,
		deleteMacroInput,
		setFieldValue,
	} = props;

	return (
		<Row className="align-content-start align-items-center row-gap-2 gx-2 pb-2">
			<Col xs="auto" style={{ width: 150 }}>
				<InputGroup size="sm">
					<Form.Control
						className="text-center"
						type="number"
						placeholder={t('InputMacroAddon:input-macro-duration-label')}
						name={`${key}.duration`}
						value={duration / (showFrames ? ONE_FRAME_US : 1000)}
						step="any"
						isInvalid={errors?.duration}
						onChange={(e) => {
							setFieldValue(
								`${key}.duration`,
								Math.round(Number(e.target.value) * (showFrames ? ONE_FRAME_US : 1000)),
							);
						}}
						min={0}
					/>
					<InputGroup.Text>
						{t(
							showFrames
								? 'InputMacroAddon:input-macro-time-label-frames'
								: 'InputMacroAddon:input-macro-time-label-ms',
						)}
					</InputGroup.Text>
				</InputGroup>
			</Col>
			{BUTTON_MASKS_OPTIONS.filter((mask) => buttonMask & mask.value).map(
				(mask, i1) => (
					<Col xs="auto" key={`${key}.buttonMask[${i1}]`}>
						<ButtonMasksComponent
							id={`${key}.buttonMask[${i1}]`}
							value={buttonMask & mask.value}
							onChange={(e) => {
								setFieldValue(
									`${key}.buttonMask`,
									(buttonMask ^ mask.value) | e.target.value,
								);
							}}
							isInvalid={errors?.buttonMask}
							translation={t}
							buttonLabelType={buttonLabelType}
							buttonMasks={BUTTON_MASKS_OPTIONS}
						/>
					</Col>
				),
			)}
			<Col xs="auto">
				<ButtonMasksComponent
					id={`${key}.buttonMaskPlaceholder`}
					className="col-sm-auto"
					value={0}
					onChange={(e) => {
						setFieldValue(`${key}.buttonMask`, buttonMask | e.target.value);
					}}
					isInvalid={errors?.buttonMask}
					translation={t}
					buttonLabelType={buttonLabelType}
					buttonMasks={BUTTON_MASKS_OPTIONS}
				/>
			</Col>
			<Col xs="auto" style={{ width: 290 }}>
				<InputGroup size="sm">
					<InputGroup.Text>
						{t('InputMacroAddon:input-macro-release-and-wait-label')}
					</InputGroup.Text>
					<Form.Control
						className="text-center d-flex"
						type="number"
						placeholder={t('InputMacroAddon:input-macro-wait-duration-label')}
						name={`${key}.waitDuration`}
						value={waitDuration / (showFrames ? ONE_FRAME_US : 1000)}
						step="any"
						isInvalid={errors?.waitDuration}
						onChange={(e) => {
							setFieldValue(
								`${key}.waitDuration`,
								Math.round(Number(e.target.value) * (showFrames ? ONE_FRAME_US : 1000)),
							);
						}}
						min={0}
					/>
					<InputGroup.Text>
						{t(
							showFrames
								? 'InputMacroAddon:input-macro-time-label-frames'
								: 'InputMacroAddon:input-macro-time-label-ms',
						)}
					</InputGroup.Text>
				</InputGroup>
			</Col>
			<Col xs="auto">
				<Button size="sm" onClick={deleteMacroInput}>
					{'✕'}
				</Button>
			</Col>
		</Row>
	);
};

const MacroComponent = (props) => {
	const {
		value: {
			macroLabel,
			macroType,
			macroInputs,
			enabled,
			exclusive,
			interruptible,
			showFrames,
			useMacroTriggerButton,
			macroTriggerButton,
		},
		errors,
		handleChange,
		id: key,
		translation: t,
		index,
		buttonLabelType,
		deleteMacroInput,
		setFieldValue,
		macroList,
	} = props;

	return (
		<div key={key}>
			<Row>
				<Col sm={'auto'}>
					<Form.Check
						name={`${key}.enabled`}
						id={`${key}.enabled`}
						label={t('InputMacroAddon:input-macro-macro-enabled')}
						type="switch"
						className="form-select-sm"
						checked={enabled}
						onChange={(e) => {
							setFieldValue(`${key}.enabled`, e.target.checked ? 1 : 0);
						}}
						isInvalid={false}
					/>
				</Col>
			</Row>
			<Row className="my-2">
				<Col sm={'auto'}>{t('InputMacroAddon:macro-name')}:</Col>
				<Col sm={'auto'}>
					<Form.Control
						size="sm"
						type="text"
						placeholder={t('InputMacroAddon:input-macro-macro-label-label')}
						name={`${key}.macroLabel`}
						value={macroLabel}
						isInvalid={errors?.macroLabel}
						onChange={handleChange}
						maxLength={64}
					/>
				</Col>
			</Row>
			<Row className="my-2">
				<Col sm={'auto'} mb={2}>
					{t('InputMacroAddon:macro-activation-type')}:
				</Col>
				<Col sm={'auto'}>
					<Form.Select
						name={`${key}.macroType`}
						className="form-select-sm sm-1"
						value={macroType}
						onChange={(e) => {
							setFieldValue(`${key}.macroType`, parseInt(e.target.value));
						}}
					>
						{MACRO_TYPES.map((o, i2) => (
							<option key={`${key}-macroType${i2}`} value={o.value}>
								{t(o.label)}
							</option>
						))}
					</Form.Select>
				</Col>
			</Row>

			<hr className="mt-4" />

			<Row>
				<Col sm={'auto'}>
					<Form.Check
						name={`${key}.interruptible`}
						label={t('InputMacroAddon:input-macro-macro-interruptible')}
						type="switch"
						className="form-select-sm"
						checked={interruptible}
						onChange={(e) => {
							setFieldValue(`${key}.interruptible`, e.target.checked ? 1 : 0);
						}}
						isInvalid={false}
					/>
				</Col>
			</Row>
			<Row>
				<Col sm={'auto'}>
					<Form.Check
						name={`${key}.exclusive`}
						label={t('InputMacroAddon:input-macro-macro-exclusive')}
						type="switch"
						className="form-select-sm"
						disabled={interruptible}
						checked={exclusive}
						onChange={(e) => {
							setFieldValue(`${key}.exclusive`, e.target.checked ? 1 : 0);
						}}
						isInvalid={false}
					/>
				</Col>
			</Row>
			<Row mt={2} className="align-items-center">
				<Col sm={'auto'}>
					<Form.Check
						name={`${key}.useMacroTriggerButton`}
						id={`${key}.useMacroTriggerButton`}
						label={t('InputMacroAddon:input-macro-macro-uses-buttons')}
						type="switch"
						className="form-select-sm"
						checked={useMacroTriggerButton}
						onChange={(e) => {
							setFieldValue(
								`${key}.useMacroTriggerButton`,
								e.target.checked ? 1 : 0,
							);
						}}
						isInvalid={false}
					/>
				</Col>
				{useMacroTriggerButton == true && (
					<Row>
						<Col sm={'auto'}>
							{t('InputMacroAddon:input-macro-macro-button-pin-plus')}
						</Col>
						<Col sm={'auto'}>
							<ButtonMasksComponent
								id={`${key}.macroTriggerButton`}
								className="col-sm-auto"
								value={macroTriggerButton}
								onChange={(e) => {
									setFieldValue(
										`${key}.macroTriggerButton`,
										parseInt(e.target.value),
									);
								}}
								buttonLabelType={buttonLabelType}
								translation={t}
								buttonMasks={BUTTON_MASKS_OPTIONS.filter(
									(b) => b.value === macroTriggerButton ||
										macroList.find(
											(m, macroIdx) =>
												index != macroIdx &&
												m.useMacroTriggerButton &&
												m.macroTriggerButton === b.value,
										) === undefined,
								)}
							/>
						</Col>
					</Row>
				)}
			</Row>
			<Tabs defaultActiveKey="editor" className="mt-3 mb-3 pb-0" fill>
				<Tab
					eventKey="editor"
					title={t('InputMacroAddon:input-macro-editor-tab')}
				>
					<Row>
						<Col sm={'auto'}>
							<Form.Check
								name={`${key}.showFrames`}
								label={t('InputMacroAddon:input-macro-macro-show-frames')}
								type="switch"
								className="form-select-sm"
								checked={showFrames}
								onChange={(e) => {
									setFieldValue(`${key}.showFrames`, e.target.checked ? 1 : 0);
								}}
								isInvalid={false}
							/>
						</Col>
					</Row>
					{macroInputs.map((macroInput, a) => (
						<MacroInputComponent
							key={`${key}.macroInputs[${a}]`}
							id={`${key}.macroInputs[${a}]`}
							value={macroInput}
							errors={errors?.macroInputs?.at(a)}
							showFrames={showFrames}
							translation={t}
							buttonLabelType={buttonLabelType}
							deleteMacroInput={() => deleteMacroInput(a)}
							handleChange={handleChange}
							setFieldValue={setFieldValue}
						/>
					))}
					{!Array.isArray(errors?.macroInputs) && errors?.macroInputs && (
						<Alert variant="danger" className="mt-2">
							{errors.macroInputs}
						</Alert>
					)}
					<Row>
						<Col sm={3}>
							{macroInputs.length < MACRO_INPUTS_MAX && (
								<Button
									variant="success"
									className="col px-2"
									size="sm"
									onClick={() => {
										setFieldValue(`${key}.macroInputs[${macroInputs.length}]`, {
											...defaultMacroInput,
										});
									}}
								>
									<Trans
										ns="InputMacroAddon"
										i18nKey="input-macro-add-input-label"
									/>
								</Button>
							)}
						</Col>
					</Row>
				</Tab>
				<Tab
					eventKey="advanced"
					title={t('InputMacroAddon:input-macro-advanced-tab')}
				>
					<Form.Control
						as="textarea"
						value={JSON.stringify(macroInputs) || ''}
						isInvalid={errors?.macroInputs}
						isValid={!errors?.macroInputs}
						onChange={(e) => {
							e.preventDefault();
							if (!e.target.value.length) {
								setFieldValue(`${key}.macroInputs`, []);
								return;
							}
							try {
								const parsed = JSON.parse(e.target.value);
								if (!Array.isArray(parsed) || parsed.length > MACRO_INPUTS_MAX ||
									parsed.some((input) => !input || typeof input !== 'object')) return;
								setFieldValue(`${key}.macroInputs`, parsed);
							} catch (error) {
								console.error('Invalid JSON', error);
							}
						}}
						rows={15}
					/>
				</Tab>
			</Tabs>
		</div>
	);
};

export default function MacrosPage() {
	const { buttonLabels } = useContext(AppContext);
	const [saveMessage, setSaveMessage] = useState('');

	const saveSettings = async (values) => {
		let success = false;
		try {
			success = Boolean(await WebApi.setMacroAddonOptions(schema.cast(values)));
		} catch (error) {
			console.error(error);
		}
		if (success && isEqual(getMacroDraft(), values)) setMacroDraft(null);
		setSaveMessage(
			success
				? t('Common:saved-success-message')
				: t('Common:saved-error-message'),
		);
	};

	const onSuccess = async (values) => await saveSettings(values);

	const { buttonLabelType, swapTpShareLabels } = buttonLabels;
	const CURRENT_BUTTONS = getButtonLabels(buttonLabelType, swapTpShareLabels);
	const buttonNames = omit(CURRENT_BUTTONS, ['label', 'value']);

	const { t } = useTranslation('');

	return (
		<Formik
			validationSchema={schema}
			onSubmit={onSuccess}
			initialValues={getMacroDraft() || defaultValues}
		>
			{({
				handleSubmit,
				handleChange,
				values,
				errors,
				setFieldValue,
				setValues,
			}) => (
				<div>
					<Form noValidate onSubmit={handleSubmit}>
						<p className="alert alert-warning" id="psx-shared-macro-warning">マクロ設定は全プロファイル共通です。TXT読込はRAMのみで、保存が成功するまで機器へ書き込みません。</p>
						<Tab.Container defaultActiveKey="settings">
							<Row>
								<Col md={3}>
									<Nav variant="pills" className="flex-column text-nowrap">
										<Nav.Item key="pills-header">
											<Nav.Link eventKey="settings">
												{t('InputMacroAddon:input-macro-header-text')}
											</Nav.Link>
										</Nav.Item>
										{values.macroList.map((macro, i) => (
											<Nav.Item key={`pills-item-${i}`}>
												<Nav.Link eventKey={`macro-${i}`}>
													{macro.macroLabel.length == 0
														? t('InputMacroAddon:input-macro-macro-list-txt', {
																macroNumber: i + 1,
															})
														: macro.macroLabel.length > 24
															? macro.macroLabel.substr(0, 24) + '...'
															: macro.macroLabel}
												</Nav.Link>
											</Nav.Item>
										))}
									</Nav>
								</Col>
								<Col md={9}>
									<Tab.Content>
										<Tab.Pane eventKey="settings">
											<Section
												title={t('InputMacroAddon:input-macro-header-text')}
											>
												<Row>
													<Col>
                                                        <Table
                                                            responsive
                                                            striped
															bordered
															hover
															className="text-center"
														>
															<thead>
																<tr>
																	<th>#</th>
																	<th>
																		{t('InputMacroAddon:table-thread-label')}
																	</th>
																	<th>
																		{t('InputMacroAddon:table-thread-type')}
																	</th>
																	<th>
																		{t(
																			'InputMacroAddon:table-thread-assigned-to',
																		)}
																	</th>
																	<th>
																		{t('InputMacroAddon:table-thread-button')}
																	</th>
																	<th>
																		{t('InputMacroAddon:table-thread-actions')}
																	</th>
																	<th>
																		{t('InputMacroAddon:table-thread-status')}
																	</th>
																</tr>
															</thead>
															<tbody>
																{values.macroList.map((macro, i) => (
																	<tr key={`macro-list-item-${i}`}>
																		<td>{i + 1}</td>
																		<td>
																			{macro.macroLabel.length == 0 && (
																				<em>None</em>
																			)}
																			{macro.macroLabel.length > 0 &&
																				macro.macroLabel.slice(0, 32)}
																			{macro.macroLabel.length > 32 && '...'}
																		</td>
																		<td>
																			{t(
																				MACRO_TYPES.find(
																				(m) => m.value === macro.macroType,
																				)?.label || MACRO_TYPES[0].label,
																			)}
																		</td>
																		<td>
																			{macro.useMacroTriggerButton == 1
																				? t(
																						'InputMacroAddon:input-macro-macro-trigger-type-button',
																					)
																				: t(
																						'InputMacroAddon:input-macro-macro-trigger-type-action',
																					)}
																		</td>
																		{macro.useMacroTriggerButton == 0 ? (
																			<td>
																				{`Macro ${i + 1}`}
																			</td>
																		) : (
																			<td>{`${
																				BUTTON_MASKS_OPTIONS.find(
																					(b) =>
																					b.value == macro.macroTriggerButton,
																				)?.label || 'None'
																			}`}</td>
																		)}
																		<td>{macro.macroInputs.length}</td>
																		<td>
																			{macro.enabled == true ? (
																				<Badge bg="success">
																					{t(
																						'InputMacroAddon:input-macro-macro-enabled-badge',
																					)}
																				</Badge>
																			) : (
																				<Badge bg="danger">
																					{t(
																						'InputMacroAddon:input-macro-macro-disabled-badge',
																					)}
																				</Badge>
																			)}
																		</td>
																	</tr>
																))}
															</tbody>
														</Table>
													</Col>
												</Row>
												<hr className="mt-3" />
												<Row>
													<Col>
														<Form.Label>
															<em>
																{t('InputMacroAddon:input-macro-sub-header')}
															</em>
														</Form.Label>
													</Col>
												</Row>
												<hr className="mt-3" />
												<Row>
													<Col sm={10}>
														<Button type="submit">
															{t('Common:button-save-label')}
														</Button>
														{saveMessage ? (
															<span className="alert">{saveMessage}</span>
														) : null}
													</Col>
												</Row>
											</Section>
										</Tab.Pane>
										{values.macroList.map((macro, i) => (
											<Tab.Pane
												key={`macro-list-tab-pane-${i}`}
												eventKey={`macro-${i}`}
											>
												<Section
													title={t(
														'InputMacroAddon:input-macro-macro-list-txt',
														{ macroNumber: i + 1 },
													)}
												>
													<MacroComponent
														key={`macroList[${i}]`}
														id={`macroList[${i}]`}
														value={values.macroList?.at(i)}
														errors={errors?.macroList?.at(i)}
														translation={t}
														buttonLabelType={buttonLabelType}
														handleChange={handleChange}
														index={i}
														setFieldValue={setFieldValue}
															deleteMacroInput={(inputIndex) => {
																setFieldValue(`macroList[${i}].macroInputs`,
																	macro.macroInputs.filter((_, index) => index !== inputIndex));
														}}
														buttonNames={buttonNames}
														macroList={values.macroList}
													/>
													<hr className="mt-3" />
													<Button type="submit">
														{t('Common:button-save-label')}
													</Button>
													{saveMessage ? (
														<span className="alert">{saveMessage}</span>
													) : null}
												</Section>
											</Tab.Pane>
										))}
									</Tab.Content>
								</Col>
							</Row>
						</Tab.Container>
						<FormContext />
					</Form>
				</div>
			)}
		</Formik>
	);
}
