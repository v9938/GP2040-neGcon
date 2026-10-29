import { create } from 'zustand';
import { baseUrl } from '../Services/WebApi';
import Http from '../Services/Http';

const percentage = (x: number, y: number): number =>
	parseFloat(((x / y) * 100).toFixed(2));
const toKB = (x: number): number => parseFloat((x / 1024).toFixed(2));

type State = {
	latestVersion: string;
	latestDownloadUrl: string;
	currentVersion: string;
	boardConfigProperties: {
		label: string;
		fileName: string;
	};
	memoryReport: {
		percentageFlash: number;
		percentageHeap: number;
		physicalFlash: number;
		staticAllocs: number;
		totalFlash: number;
		totalHeap: number;
		usedFlash: number;
		usedHeap: number;
	};
	stats: {
		architecture: string;
		build: string;
		buildType: string;
	};
	loading: boolean;
	error: boolean;
};

type Actions = {
	getSystemStats: () => void;
};

const INITIAL_STATE: State = {
	latestVersion: '',
	latestDownloadUrl: '',
	currentVersion: '',
	boardConfigProperties: {
		label: '',
		fileName: '',
	},
	memoryReport: {
		percentageFlash: 0,
		percentageHeap: 0,
		physicalFlash: 0,
		staticAllocs: 0,
		totalFlash: 0,
		totalHeap: 0,
		usedFlash: 0,
		usedHeap: 0,
	},
	stats: {
		architecture: '',
		build: '',
		buildType: '',
	},
	loading: false,
	error: false,
};

const useSystemStats = create<State & Actions>()((set) => ({
	...INITIAL_STATE,
	getSystemStats: async () => {
		set({ loading: true });

		try {
            const [firmwareVersion, memoryReport] = await Promise.all([
				Http.get(`${baseUrl}/api/getFirmwareVersion`).then((res) => res.data),
				Http.get(`${baseUrl}/api/getMemoryReport`).then((res) => res.data),
			]);
            // この専用FWは公式の汎用UF2へ更新しない。NDIS/PCプレビューとも外部通信不要。
            const latestDownloadUrl = '';

			set({
				currentVersion: firmwareVersion.version,
                latestVersion: '',
				latestDownloadUrl,
				boardConfigProperties: {
					label: firmwareVersion.boardConfigLabel,
					fileName: firmwareVersion.boardConfigFileName,
				},
				memoryReport: {
					physicalFlash: toKB(memoryReport.physicalFlash),
					staticAllocs: toKB(memoryReport.staticAllocs),
					totalFlash: toKB(memoryReport.totalFlash),
					totalHeap: toKB(memoryReport.totalHeap),
					usedFlash: toKB(memoryReport.usedFlash),
					usedHeap: toKB(memoryReport.usedHeap),
					percentageFlash: percentage(
						memoryReport.usedFlash,
						memoryReport.totalFlash,
					),
					percentageHeap: percentage(
						memoryReport.usedHeap,
						memoryReport.totalHeap,
					),
				},
				stats: {
					architecture: firmwareVersion.boardArchitecture,
					build: firmwareVersion.boardBuild,
					buildType: firmwareVersion.boardBuildType,
				},
				loading: false,
			});
		} catch (error) {
			set({ error: true, loading: false });
		}
	},
}));

export default useSystemStats;
