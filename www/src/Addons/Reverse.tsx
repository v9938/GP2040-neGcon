import { useTranslation } from 'react-i18next';
import { FormCheck, Row } from 'react-bootstrap';
import * as yup from 'yup';

import Section from '../Components/Section';
import FormSelect from '../Components/FormSelect';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

const REVERSE_ACTION = [
	{ label: 'Ignore', value: 0 },
	{ label: 'Invert', value: 1 },
	{ label: 'Neutral', value: 2 },
];

export const reverseScheme = {
	ReverseInputEnabled: yup.number().required().oneOf([0, 1]).label('Reverse Input Enabled'),
	reverseActionUp: yup.number().required().oneOf([0, 1, 2]),
	reverseActionDown: yup.number().required().oneOf([0, 1, 2]),
	reverseActionLeft: yup.number().required().oneOf([0, 1, 2]),
	reverseActionRight: yup.number().required().oneOf([0, 1, 2]),
};

export const reverseState = {
	ReverseInputEnabled: 0,
	reverseActionUp: 0,
	reverseActionDown: 0,
	reverseActionLeft: 0,
	reverseActionRight: 0,
};

const Reverse = ({ values, errors, handleChange, handleCheckbox }: AddonPropTypes) => {
	const { t } = useTranslation();
	const translatedReverseAction = REVERSE_ACTION.map((option) => ({
		...option,
		label: t(`AddonsConfig:reverse-action-${option.label.toLowerCase()}-label`),
	}));
	return (
		<Section title={
			<a
				href="https://gp2040-ce.info/add-ons/input-reverse"
				target="_blank"
				className="text-reset text-decoration-none"
			>
				{t('AddonsConfig:input-reverse-header-text')}
			</a>
		}
		>
			<div id="ReverseInputOptions" hidden={!values.ReverseInputEnabled}>
				<p>{t('AddonsConfig:reverse-psx-guide')}</p>
				<Row className="mb-3">
					<FormSelect
						label={t('AddonsConfig:input-reverse-action-up-label')}
						name="reverseActionUp"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.reverseActionUp}
						error={errors.reverseActionUp}
						isInvalid={Boolean(errors.reverseActionUp)}
						onChange={handleChange}
					>
						{translatedReverseAction.map((o, i) => (
							<option key={`reverseActionUp-option-${i}`} value={o.value}>
								{o.label}
							</option>
						))}
					</FormSelect>
					<FormSelect
						label={t('AddonsConfig:input-reverse-action-down-label')}
						name="reverseActionDown"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.reverseActionDown}
						error={errors.reverseActionDown}
						isInvalid={Boolean(errors.reverseActionDown)}
						onChange={handleChange}
					>
						{translatedReverseAction.map((o, i) => (
							<option key={`reverseActionDown-option-${i}`} value={o.value}>
								{o.label}
							</option>
						))}
					</FormSelect>
					<FormSelect
						label={t('AddonsConfig:input-reverse-action-left-label')}
						name="reverseActionLeft"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.reverseActionLeft}
						error={errors.reverseActionLeft}
						isInvalid={Boolean(errors.reverseActionLeft)}
						onChange={handleChange}
					>
						{translatedReverseAction.map((o, i) => (
							<option key={`reverseActionLeft-option-${i}`} value={o.value}>
								{o.label}
							</option>
						))}
					</FormSelect>
					<FormSelect
						label={t('AddonsConfig:input-reverse-action-right-label')}
						name="reverseActionRight"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.reverseActionRight}
						error={errors.reverseActionRight}
						isInvalid={Boolean(errors.reverseActionRight)}
						onChange={handleChange}
					>
						{translatedReverseAction.map((o, i) => (
							<option key={`reverseActionRight-option-${i}`} value={o.value}>
								{o.label}
							</option>
						))}
					</FormSelect>
				</Row>
			</div>
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="ReverseInputButton"
				reverse
				isInvalid={false}
				checked={Boolean(values.ReverseInputEnabled)}
				onChange={(e) => {
					handleCheckbox('ReverseInputEnabled');
					handleChange(e);
				}}
			/>
		</Section>
	);
};

export default Reverse;
