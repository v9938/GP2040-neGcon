import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';

import { AppContextProvider } from './Contexts/AppContext';

import Navigation from './Components/Navigation';

import HomePage from './Pages/HomePage';
import PinMappingPage from './Pages/PinMapping';
import ResetSettingsPage from './Pages/ResetSettingsPage';
import SettingsPage from './Pages/SettingsPage';
import AddonsConfigPage from './Pages/AddonsConfigPage';
import BackupPage from './Pages/BackupPage';
import MacrosPage from './Pages/InputMacroAddonPage';
import AnalogMonitor from './Pages/AnalogMonitor';

import './App.scss';

const App = () => {
	return (
		<AppContextProvider>
			<Router>
				<Navigation />
				<div className="body-content container-lg">
					<Routes>
						<Route path="/" element={<HomePage />} />
						<Route path="/settings" element={<SettingsPage />} />
						<Route path="/pin-mapping" element={<PinMappingPage />} />
						<Route path="/reset-settings" element={<ResetSettingsPage />} />
						<Route path="/add-ons" element={<AddonsConfigPage />} />
						<Route path="/backup" element={<BackupPage />} />
						<Route path="/macro" element={<MacrosPage />} />
						<Route path="/input-macro" element={<MacrosPage />} />
						<Route path="/analog-monitor" element={<AnalogMonitor />} />
					</Routes>
				</div>
			</Router>
		</AppContextProvider>
	);
};

export default App;
