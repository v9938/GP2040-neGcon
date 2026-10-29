// API の pinNN は GPIO ではなく PSX 入力の仮想スロット。
export const PSX_INPUTS = [
	{ key: 'pin08', slot: 8, label: 'UP' },
	{ key: 'pin09', slot: 9, label: 'DOWN' },
	{ key: 'pin10', slot: 10, label: 'RIGHT' },
	{ key: 'pin11', slot: 11, label: 'LEFT' },
	{ key: 'pin12', slot: 12, label: 'CROSS (I)' },
	{ key: 'pin13', slot: 13, label: 'CIRCLE (A)' },
	{ key: 'pin14', slot: 14, label: 'R2' },
	{ key: 'pin15', slot: 15, label: 'L2' },
	{ key: 'pin16', slot: 16, label: 'SQUARE (II)' },
	{ key: 'pin17', slot: 17, label: 'TRIANGLE (B)' },
	{ key: 'pin18', slot: 18, label: 'R1 (R)' },
	{ key: 'pin19', slot: 19, label: 'L1 (L)' },
	{ key: 'pin20', slot: 20, label: 'SELECT' },
	{ key: 'pin21', slot: 21, label: 'START' },
	{ key: 'pin22', slot: 22, label: 'L3' },
	{ key: 'pin23', slot: 23, label: 'R3' },
] as const;
