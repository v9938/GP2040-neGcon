// This is modeled after Axios to make an easier transition, so this may need more buildout
// if more than simple GET and POST requests are required.

type GetOptions = {
	headers?: Record<string, string>;
	signal?: AbortSignal;
};

class Http {
	async get(url: string, { headers = {}, signal }: GetOptions = {}) {
		try {
			const response = await fetch(url, {
				method: 'GET',
				headers: {
					'Content-Type': 'application/json',
					...headers,
				},
				signal,
				cache: 'no-store',
			});

			if (!response.ok) throw new Error(`HTTP ${response.status}: ${url}`);
			const json = await response.json();
			if (json == null || json.error !== undefined || json.success === false)
				throw new Error(json?.error || `API request failed: ${url}`);
			return Promise.resolve({ data: json });
		} catch (err) {
			return Promise.reject(err);
		}
	}

	async post(url: string, body: unknown, headers: object = {}) {
		try {
			const response = await fetch(url, {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json',
					...headers,
				},
				body: JSON.stringify(body),
			});

			if (!response.ok) throw new Error(`HTTP ${response.status}: ${url}`);
			const json = await response.json();
			if (json == null || json.error !== undefined || json.success === false)
				throw new Error(json?.error || `API request failed: ${url}`);
			return Promise.resolve({ data: json });
		} catch (err) {
			return Promise.reject(err);
		}
	}
}

export default new Http();
