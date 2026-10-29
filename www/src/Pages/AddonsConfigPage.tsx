import { Dispatch, SetStateAction, useContext, useEffect, useState } from 'react';
import { Button, Form } from 'react-bootstrap';
import { Formik, FormikErrors, FormikHandlers, FormikHelpers, useFormikContext } from 'formik';
import * as yup from 'yup';
import { useTranslation } from 'react-i18next';

import isEqual from 'lodash/isEqual';
import pick from 'lodash/pick';

import { AppContext } from '../Contexts/AppContext';

import WebApi from '../Services/WebApi';
import { getAddonDraft, setAddonDraft, PSX_PRESET_STAGED_EVENT } from '../Services/PsxDraft';
import Analog, { analogScheme, analogState } from '../Addons/Analog';
import Bootsel, { bootselScheme, bootselState } from '../Addons/Bootsel';
import OnBoardLed, {
	onBoardLedScheme,
	onBoardLedState,
} from '../Addons/OnBoardLed';
import Reverse, { reverseScheme, reverseState } from '../Addons/Reverse';
import Keyboard, { keyboardScheme, keyboardState } from '../Addons/Keyboard';
import GamepadUSBHost, {
	gamepadUSBHostScheme,
	gamepadUSBHostState,
} from '../Addons/GamepadUSBHost';
import ReactiveLED, {
	reactiveLEDScheme,
	reactiveLEDState,
} from '../Addons/ReactiveLED';

export type AddonPropTypes = {
	values: typeof DEFAULT_VALUES;
	errors: FormikErrors<typeof DEFAULT_VALUES>;
	handleChange: FormikHandlers['handleChange'];
	handleCheckbox: (name: keyof typeof DEFAULT_VALUES) => void;
	setFieldValue: FormikHelpers<typeof DEFAULT_VALUES>['setFieldValue'];
};

const schema = yup.object().shape({
	...analogScheme,
	...onBoardLedScheme,
	...keyboardScheme,
	...reactiveLEDScheme,
	...gamepadUSBHostScheme,
	...bootselScheme,
	...reverseScheme,
	InputMacroAddonEnabled: yup.number().required().oneOf([0, 1]),
});

export const DEFAULT_VALUES = {
	...analogState,
	...bootselState,
	...onBoardLedState,
	...reverseState,
	...keyboardState,
	...reactiveLEDState,
	...gamepadUSBHostState,
	InputMacroAddonEnabled: 0,
} as const;

const ADDONS = [
	Analog,
	Keyboard,
	GamepadUSBHost,
	OnBoardLed,
	ReactiveLED,
	Bootsel,
	Reverse,
];

const FormContext = ({ setStoredData }: { setStoredData: Dispatch<SetStateAction<Record<string, unknown>>> }) => {
	const { setValues, values } = useFormikContext<typeof DEFAULT_VALUES>();
	const { setLoading } = useContext(AppContext);
	const [loaded, setLoaded] = useState(false);

	useEffect(() => {
		let mounted = true;
		const applyDraft = () => {
			const draft = getAddonDraft();
			if (draft) setValues((current) => ({ ...current, ...pick(draft, Object.keys(DEFAULT_VALUES)), AnalogInputEnabled: 1 }));
		};
		window.addEventListener(PSX_PRESET_STAGED_EVENT, applyDraft);
		async function fetchData() {
			const data = await WebApi.getAddonsOptions(setLoading);
			if (!mounted) return;
			if (data) setStoredData(structuredClone(data));
			setValues({ ...DEFAULT_VALUES, ...pick(data || {}, Object.keys(DEFAULT_VALUES)),
				...pick(getAddonDraft() || {}, Object.keys(DEFAULT_VALUES)), AnalogInputEnabled: 1 });
			setLoaded(true);
		}
		fetchData().catch(console.error);
		const reload = () => fetchData().catch(console.error);
		window.addEventListener('psx-preset-applied', reload);
		return () => { mounted = false; window.removeEventListener(PSX_PRESET_STAGED_EVENT, applyDraft); window.removeEventListener('psx-preset-applied', reload); };
	}, [setValues]);
	useEffect(() => { if (loaded) setAddonDraft(values); }, [loaded, values]);

	return null;
};

export default function AddonsConfigPage() {
	const { updateUsedPins, updatePeripherals } = useContext(AppContext);
	const [saveMessage, setSaveMessage] = useState('');
	const [storedData, setStoredData] = useState<Record<string, unknown>>({});
	const [initialValues] = useState(() => ({ ...DEFAULT_VALUES,
		...pick(getAddonDraft() || {}, Object.keys(DEFAULT_VALUES)), AnalogInputEnabled: 1 as const }));

	const { t } = useTranslation();

	useEffect(() => {
		updatePeripherals().catch(console.error);
	}, []);

	const onSuccess = async (values: typeof DEFAULT_VALUES) => {
		// 新規キーも比較する。小数は schema の number cast で保持し、Formik を直接変更しない。
		const data = {
			...schema.cast({ ...pick(values, Object.keys(schema.fields)), AnalogInputEnabled: 1 }),
			keyboardHostMap: values.keyboardHostMap,
		};
		const resultObject = Object.fromEntries(
			Object.entries(data).filter(([key, value]) => !isEqual(value, storedData[key])),
		);
		let success = false;
		try {
			success = await WebApi.setAddonsOptions(resultObject);
		} catch (error) {
			console.error(error);
		}
		// 失敗時は baseline を変更せず、同じ差分を再送できるようにする。
		if (success) {
			setStoredData(JSON.parse(JSON.stringify(data)));
			if (isEqual(getAddonDraft(), values)) setAddonDraft(null);
		}
		setSaveMessage(
			success
				? t('Common:saved-success-message')
				: t('Common:saved-error-message'),
		);
		if (success) updateUsedPins().catch(console.error);
	};

	return (
		<Formik
			enableReinitialize={true}
			validationSchema={schema}
			onSubmit={onSuccess}
			initialValues={initialValues}
		>
			{({ handleSubmit, handleChange, values, errors, setFieldValue }) => (
				<Form noValidate onSubmit={handleSubmit}>
					<h1>{t('AddonsConfig:header-text')}</h1>
					<p>{t('AddonsConfig:sub-header-text')}</p>
					<p className="alert alert-warning" id="psx-shared-addons-warning">ADDON設定は全プロファイル共通です。TXT読込・画面移動はRAMのみで、保存が成功するまで機器へ書き込みません。LEDとマクロは各保存ボタン、読込一括保存はピン設定画面を使用します。</p>
					{ADDONS.map((Addon, index) => (
						<Addon
							key={`addon-${index}`}
							values={values}
							errors={errors}
							handleChange={handleChange}
							handleCheckbox={(name: keyof typeof DEFAULT_VALUES) => {
								setFieldValue(name, values[name] === 1 ? 0 : 1);
							}}
							setFieldValue={setFieldValue}
						/>
					))}
					<Form.Check
						id="InputMacroAddonEnabled"
						name="InputMacroAddonEnabled"
						type="switch"
						label={t('InputMacroAddon:input-macro-header-text')}
						checked={Boolean(values.InputMacroAddonEnabled)}
						onChange={(e) => setFieldValue('InputMacroAddonEnabled', e.target.checked ? 1 : 0)}
					/>

					<div className="mt-3">
						<Button type="submit" id="save">
							{t('Common:button-save-label')}
						</Button>
						{saveMessage ? <span className="alert">{saveMessage}</span> : null}
					</div>
					<FormContext setStoredData={setStoredData} />
				</Form>
			)}
		</Formik>
	);
}
