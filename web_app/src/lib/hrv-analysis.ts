/**
 * HRV Analysis Module
 * Calcula métricas de variabilidade cardíaca a partir de waveform PPG
 */

export interface HRVMetrics {
    bpm: number;
    sdnn: number;
    rmssd: number;
    pnn50: number;
    rrIntervals: number[];
    peakIndices: number[];
}

/**
 * Detecta picos no sinal PPG usando derivada e threshold adaptativo
 */
export function detectPeaks(signal: number[], sampleRate: number): number[] {
    if (!signal || signal.length < 100) return [];

    // Parâmetros
    const minDistance = Math.floor(sampleRate * 0.4); // Mínimo 400ms entre picos (~150 BPM max)
    const windowSize = Math.floor(sampleRate * 2); // Janela de 2 segundos para threshold

    // Normalizar sinal
    const min = Math.min(...signal);
    const max = Math.max(...signal);
    const normalized = signal.map(v => (v - min) / (max - min));

    // Suavizar com média móvel
    const smoothed = movingAverage(normalized, Math.floor(sampleRate / 50));

    // Encontrar picos locais
    const peaks: number[] = [];

    for (let i = minDistance; i < smoothed.length - minDistance; i++) {
        // Calcular threshold local
        const windowStart = Math.max(0, i - windowSize);
        const windowEnd = Math.min(smoothed.length, i + windowSize);
        const windowData = smoothed.slice(windowStart, windowEnd);
        const localMean = windowData.reduce((a, b) => a + b, 0) / windowData.length;
        const localMax = Math.max(...windowData);
        const threshold = localMean + (localMax - localMean) * 0.5;

        // Verificar se é pico local
        if (smoothed[i] > threshold) {
            let isPeak = true;
            for (let j = i - Math.floor(minDistance / 2); j <= i + Math.floor(minDistance / 2); j++) {
                if (j !== i && j >= 0 && j < smoothed.length && smoothed[j] > smoothed[i]) {
                    isPeak = false;
                    break;
                }
            }

            if (isPeak) {
                // Verificar distância do último pico
                if (peaks.length === 0 || i - peaks[peaks.length - 1] >= minDistance) {
                    peaks.push(i);
                }
            }
        }
    }

    return peaks;
}

/**
 * Média móvel simples
 */
function movingAverage(data: number[], windowSize: number): number[] {
    const result: number[] = [];
    for (let i = 0; i < data.length; i++) {
        const start = Math.max(0, i - Math.floor(windowSize / 2));
        const end = Math.min(data.length, i + Math.floor(windowSize / 2) + 1);
        const window = data.slice(start, end);
        result.push(window.reduce((a, b) => a + b, 0) / window.length);
    }
    return result;
}

/**
 * Calcula intervalos RR em milissegundos
 */
export function calculateRRIntervals(peakIndices: number[], sampleRate: number): number[] {
    const rrIntervals: number[] = [];

    for (let i = 1; i < peakIndices.length; i++) {
        const rrMs = ((peakIndices[i] - peakIndices[i - 1]) / sampleRate) * 1000;

        // Filtrar intervalos fisiologicamente plausíveis (300ms a 2000ms = 30-200 BPM)
        if (rrMs >= 300 && rrMs <= 2000) {
            rrIntervals.push(rrMs);
        }
    }

    return rrIntervals;
}

/**
 * Calcula métricas de HRV a partir dos intervalos RR
 */
export function calculateHRVMetrics(rrIntervals: number[]): Omit<HRVMetrics, 'peakIndices'> & { peakIndices: number[] } {
    if (rrIntervals.length < 5) {
        return {
            bpm: 0,
            sdnn: 0,
            rmssd: 0,
            pnn50: 0,
            rrIntervals: [],
            peakIndices: []
        };
    }

    // BPM médio
    const meanRR = rrIntervals.reduce((a, b) => a + b, 0) / rrIntervals.length;
    const bpm = 60000 / meanRR;

    // SDNN - Desvio padrão de todos os intervalos NN
    const sdnn = Math.sqrt(
        rrIntervals.reduce((sum, rr) => sum + Math.pow(rr - meanRR, 2), 0) / rrIntervals.length
    );

    // RMSSD - Raiz quadrada da média dos quadrados das diferenças sucessivas
    let sumSquaredDiff = 0;
    for (let i = 1; i < rrIntervals.length; i++) {
        sumSquaredDiff += Math.pow(rrIntervals[i] - rrIntervals[i - 1], 2);
    }
    const rmssd = Math.sqrt(sumSquaredDiff / (rrIntervals.length - 1));

    // pNN50 - Porcentagem de intervalos com diferença > 50ms
    let nn50Count = 0;
    for (let i = 1; i < rrIntervals.length; i++) {
        if (Math.abs(rrIntervals[i] - rrIntervals[i - 1]) > 50) {
            nn50Count++;
        }
    }
    const pnn50 = (nn50Count / (rrIntervals.length - 1)) * 100;

    return {
        bpm: Math.round(bpm * 10) / 10,
        sdnn: Math.round(sdnn * 10) / 10,
        rmssd: Math.round(rmssd * 10) / 10,
        pnn50: Math.round(pnn50 * 10) / 10,
        rrIntervals,
        peakIndices: []
    };
}

/**
 * Analisa waveform completa e retorna métricas de HRV
 */
export function analyzeWaveform(irWaveform: number[], sampleRate: number): HRVMetrics {
    const peakIndices = detectPeaks(irWaveform, sampleRate);
    const rrIntervals = calculateRRIntervals(peakIndices, sampleRate);
    const metrics = calculateHRVMetrics(rrIntervals);

    return {
        ...metrics,
        peakIndices
    };
}
