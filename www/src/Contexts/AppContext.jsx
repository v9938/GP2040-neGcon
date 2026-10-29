import { createContext, useEffect, useState } from 'react';
import * as yup from 'yup';

import WebApi, { basePeripheralMapping } from '../Services/WebApi';

export const AppContext = createContext(null);

let checkPins = null;


yup.addMethod(yup.string, 'validateUSBHexID', function () {
	return this.test('', 'Valid USB hex ID required', (value) =>
		value?.match(/^([0-9a-f]{4})$/i),
	);
});

yup.addMethod(
	yup.NumberSchema,
	'validateSelectionWhenValue',
	function (name, choices) {
		return this.when(name, {
			is: 1,
			then: () => this.required().oneOf(choices.map((o) => o.value)),
			otherwise: () => yup.mixed().notRequired(),
		});
	},
);

yup.addMethod(yup.NumberSchema, 'validateNumberWhenValue', function (name) {
	return this.when(name, {
		is: 1,
		then: () => this.required(),
		otherwise: () => yup.mixed().notRequired().strip(),
	});
});

yup.addMethod(
	yup.NumberSchema,
	'validateMinWhenEqualTo',
	function (name, compareValue, min) {
		return this.when(name, {
			is: (value) => parseInt(value) === compareValue,
			then: () => this.required().min(min),
			otherwise: () => yup.mixed().notRequired().strip(),
		});
	},
);

yup.addMethod(
	yup.NumberSchema,
	'validateRangeWhenValue',
	function (name, min, max) {
		return this.when(name, {
			is: 1,
			then: () => this.required().min(min).max(max),
			otherwise: () => yup.mixed().notRequired().strip(),
		});
	},
);

yup.addMethod(
	yup.NumberSchema,
	'validatePinWhenEqualTo',
	function (name, compareName, compareValue) {
		return this.when(compareName, {
			is: (value) => parseInt(value) === compareValue,
			then: () => this.validatePinWhenValue(name),
			otherwise: () => yup.mixed().notRequired().strip(),
		});
	},
);

yup.addMethod(yup.NumberSchema, 'validatePinWhenValue', function (name) {
	return this.when(name, {
		is: 1,
		then: () => this.checkUsedPins(),
		otherwise: () => yup.mixed().notRequired().strip(),
	});
});

yup.addMethod(yup.NumberSchema, 'checkUsedPins', function () {
	return this.test(
		'',
		'${originalValue} is unavailable/already assigned!',
		(value) => checkPins(value),
	);
});

const parseBoolean = (bool) => String(bool).toLowerCase() === 'true';

export const AppContextProvider = ({ children, ...props }) => {
	const localValue = localStorage.getItem('buttonLabelType') || 'gp2040';
	const localValue2 =
		parseBoolean(localStorage.getItem('swapTpShareLabels')) || false;
	const [buttonLabels, _setButtonLabels] = useState({
		swapTpShareLabels: localValue2,
		buttonLabelType: localValue,
	});
	const setButtonLabels = ({
		buttonLabelType: newType,
		swapTpShareLabels: newSwap,
	}) => {
		newType && localStorage.setItem('buttonLabelType', newType);
		newSwap !== undefined &&
			localStorage.setItem('swapTpShareLabels', parseBoolean(newSwap));
		_setButtonLabels(({ buttonLabelType, swapTpShareLabels }) => ({
			buttonLabelType: newType || buttonLabelType,
			swapTpShareLabels: parseBoolean(
				newSwap !== undefined ? newSwap : swapTpShareLabels,
			),
		}));
	};


	const updateButtonLabels = (e) => {
		const { key, newValue } = e;
		if (key === 'swapTpShareLabels') {
			_setButtonLabels(({ buttonLabelType }) => ({
				buttonLabelType,
				swapTpShareLabels: parseBoolean(newValue),
			}));
		}
		if (key === 'buttonLabelType') {
			_setButtonLabels(({ swapTpShareLabels }) => ({
				buttonLabelType: newValue,
				swapTpShareLabels: parseBoolean(swapTpShareLabels),
			}));
		}
	};

	useEffect(() => {
		_setButtonLabels({
			buttonLabelType: buttonLabels.buttonLabelType,
			swapTpShareLabels: parseBoolean(buttonLabels.swapTpShareLabels),
		});
		window.addEventListener('storage', updateButtonLabels);
		return () => {
			window.removeEventListener('storage', updateButtonLabels);
		};
	}, []);


	const [usedPins, setUsedPins] = useState([]);
	const [availablePeripherals, setAvailablePeripherals] = useState(
		basePeripheralMapping,
	);

	const updateUsedPins = async () => {
		const data = await WebApi.getUsedPins(setLoading);
		setUsedPins(data.usedPins);
		return data;
	};


	const updatePeripherals = async () => {
		const peripherals = await WebApi.getPeripheralOptions(setLoading);
		setAvailablePeripherals(peripherals);
	};

	useEffect(() => {
		updateUsedPins().catch(console.error);
		updatePeripherals().catch(console.error);
	}, []);

	useEffect(() => {
		checkPins = (value) => {
			const hasValue = value > -1;
			const isValid =
				value === undefined ||
				value === -1 ||
				(hasValue && value < 30 && (usedPins || []).indexOf(value) === -1);
			return isValid;
		};
	}, [usedPins, setUsedPins]);


	const getAvailablePeripherals = (device) => {
		if (device === 'usb') return [{ label: 'usb0', value: 0 }];
		return false;
	};

	useEffect(() => {}, [availablePeripherals, setAvailablePeripherals]);

	const [savedColorScheme, _setSavedColorScheme] = useState(
		localStorage.getItem('savedColorScheme') || 'auto',
	);
	const setSavedColorScheme = (savedColorScheme) => {
		localStorage.setItem('savedColorScheme', savedColorScheme);
		_setSavedColorScheme(savedColorScheme);
	};

	const [savedLanguage, _setSavedLanguage] = useState(
		localStorage.getItem('i18next') || 'en-GB',
	);
	const setSavedLanguage = (savedLanguage) => {
		localStorage.setItem('i18next', savedLanguage);
		_setSavedLanguage(savedLanguage);
	};

	const [loading, setLoading] = useState(false);

	return (
		<AppContext.Provider
			{...props}
			value={{
				buttonLabels,
				usedPins,
				availablePeripherals,
				getAvailablePeripherals,
				setButtonLabels,
				setUsedPins,
				setAvailablePeripherals,
				updatePeripherals,
				updateUsedPins,
				savedColorScheme,
				setSavedColorScheme,
				savedLanguage,
				setSavedLanguage,
				loading,
				setLoading,
			}}
		>
			{children}
		</AppContext.Provider>
	);
};
