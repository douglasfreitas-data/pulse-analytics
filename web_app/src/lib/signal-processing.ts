/**
 * Signal Processing Module
 * Pré-processamento de sinais PPG com wavelet denoising
 * 
 * Remove:
 * - Drift de baseline (respiração, movimento lento)
 * - Ruído de alta frequência
 * - Artefatos de movimento
 */

// ============================================
// WAVELET DENOISING
// ============================================

/**
 * Decomposição Wavelet usando Haar (mais simples e eficiente)
 * Retorna coeficientes de aproximação e detalhe
 */
function haarWaveletDecompose(signal: number[]): { approx: number[], detail: number[] } {
    const n = signal.length;
    const halfN = Math.floor(n / 2);
    const approx: number[] = [];
    const detail: number[] = [];

    const sqrt2 = Math.sqrt(2);

    for (let i = 0; i < halfN; i++) {
        const idx = i * 2;
        if (idx + 1 < n) {
            // Coeficiente de aproximação (média)
            approx.push((signal[idx] + signal[idx + 1]) / sqrt2);
            // Coeficiente de detalhe (diferença)
            detail.push((signal[idx] - signal[idx + 1]) / sqrt2);
        }
    }

    return { approx, detail };
}

/**
 * Reconstrução Wavelet a partir dos coeficientes
 */
function haarWaveletReconstruct(approx: number[], detail: number[]): number[] {
    const n = approx.length;
    const result: number[] = [];

    const sqrt2 = Math.sqrt(2);

    for (let i = 0; i < n; i++) {
        // Reconstruir par de amostras
        result.push((approx[i] + detail[i]) / sqrt2);
        result.push((approx[i] - detail[i]) / sqrt2);
    }

    return result;
}

/**
 * Soft thresholding para denoising
 */
function softThreshold(coefficients: number[], threshold: number): number[] {
    return coefficients.map(c => {
        if (Math.abs(c) <= threshold) return 0;
        return c > 0 ? c - threshold : c + threshold;
    });
}

/**
 * Calcula threshold universal (VisuShrink)
 */
function calculateUniversalThreshold(coefficients: number[]): number {
    const n = coefficients.length;
    // Estimativa robusta do ruído usando MAD (Median Absolute Deviation)
    const sorted = [...coefficients].map(Math.abs).sort((a, b) => a - b);
    const median = sorted[Math.floor(sorted.length / 2)];
    const sigma = median / 0.6745; // Fator de correção para distribuição normal

    return sigma * Math.sqrt(2 * Math.log(n));
}

/**
 * Wavelet Denoising multinível
 * Remove ruído de alta frequência mantendo features do PPG
 */
export function waveletDenoise(signal: number[], levels: number = 4): number[] {
    if (signal.length < 16) return signal;

    // Garantir comprimento par para cada nível
    let paddedSignal = [...signal];
    const originalLength = signal.length;
    while (paddedSignal.length % Math.pow(2, levels) !== 0) {
        paddedSignal.push(paddedSignal[paddedSignal.length - 1]);
    }

    // Decomposição multinível
    const details: number[][] = [];
    let currentApprox = paddedSignal;

    for (let level = 0; level < levels; level++) {
        const { approx, detail } = haarWaveletDecompose(currentApprox);
        details.push(detail);
        currentApprox = approx;
    }

    // Aplicar thresholding nos coeficientes de detalhe (mantém aproximação)
    const denoisedDetails = details.map((detail, level) => {
        // Threshold mais agressivo para níveis mais altos (ruído de alta freq)
        const threshold = calculateUniversalThreshold(detail) * (1 + level * 0.2);
        return softThreshold(detail, threshold);
    });

    // Reconstrução
    let reconstructed = currentApprox;
    for (let level = levels - 1; level >= 0; level--) {
        reconstructed = haarWaveletReconstruct(reconstructed, denoisedDetails[level]);
    }

    // Retornar apenas o comprimento original
    return reconstructed.slice(0, originalLength);
}

// ============================================
// REMOÇÃO DE BASELINE DRIFT
// ============================================

/**
 * Remove drift de baseline usando filtro de média móvel grande
 * Eficaz para artefatos de respiração (0.1-0.5 Hz)
 */
export function removeBaselineDrift(signal: number[], sampleRate: number): number[] {
    // Janela de ~4 segundos para capturar ciclos respiratórios completos
    const windowSize = Math.floor(sampleRate * 4);

    if (windowSize >= signal.length) {
        // Sinal muito curto, apenas centralizar
        const mean = signal.reduce((a, b) => a + b, 0) / signal.length;
        return signal.map(v => v - mean);
    }

    // Calcular baseline com média móvel grande
    const baseline: number[] = [];
    const halfWindow = Math.floor(windowSize / 2);

    for (let i = 0; i < signal.length; i++) {
        const start = Math.max(0, i - halfWindow);
        const end = Math.min(signal.length, i + halfWindow);
        const window = signal.slice(start, end);
        baseline.push(window.reduce((a, b) => a + b, 0) / window.length);
    }

    // Subtrair baseline
    return signal.map((v, i) => v - baseline[i]);
}

/**
 * Remove baseline usando wavelet (mais preciso)
 * Remove componentes de frequência muito baixa (< 0.5 Hz)
 */
export function removeBaselineWavelet(signal: number[], _sampleRate?: number): number[] {
    // Níveis de decomposição baseados na taxa de amostragem
    // Para 757 Hz, 10 níveis dá frequência de corte ~0.37 Hz
    const levels = Math.min(10, Math.floor(Math.log2(signal.length)) - 2);

    if (levels < 3) return signal;

    // Padding
    let paddedSignal = [...signal];
    const originalLength = signal.length;
    while (paddedSignal.length % Math.pow(2, levels) !== 0) {
        paddedSignal.push(paddedSignal[paddedSignal.length - 1]);
    }

    // Decomposição completa
    const details: number[][] = [];
    let currentApprox = paddedSignal;

    for (let level = 0; level < levels; level++) {
        const { approx, detail } = haarWaveletDecompose(currentApprox);
        details.push(detail);
        currentApprox = approx;
    }

    // Zerar os últimos 3 níveis de aproximação (baixas frequências = baseline)
    // Isso remove drift sem afetar o pulso cardíaco
    const zeroApprox = currentApprox.map(() => 0);

    // Reconstrução sem baseline
    let reconstructed = zeroApprox;
    for (let level = levels - 1; level >= 0; level--) {
        reconstructed = haarWaveletReconstruct(reconstructed, details[level]);
    }

    return reconstructed.slice(0, originalLength);
}

// ============================================
// FILTRO PASSA-BANDA
// ============================================

/**
 * Filtro passa-banda simples usando diferença de médias móveis
 * Mantém frequências cardíacas (0.5-4 Hz = 30-240 BPM)
 */
export function bandpassFilter(signal: number[], sampleRate: number): number[] {
    // Passa-alta: remover frequências < 0.5 Hz (baseline drift)
    const lowCutoff = 0.5; // Hz
    const highCutoff = 4.0; // Hz

    // Tamanhos de janela para os filtros
    const lowWindow = Math.floor(sampleRate / lowCutoff);
    const highWindow = Math.floor(sampleRate / highCutoff);

    // Aplicar passa-alta (subtrair média móvel grande)
    const baseline = movingAverageSmooth(signal, Math.min(lowWindow, signal.length - 1));
    const highPassed = signal.map((v, i) => v - baseline[i]);

    // Aplicar passa-baixa (suavizar com média móvel pequena)
    return movingAverageSmooth(highPassed, Math.max(3, highWindow));
}

/**
 * Média móvel otimizada
 */
function movingAverageSmooth(data: number[], windowSize: number): number[] {
    if (windowSize < 1) windowSize = 1;
    if (windowSize > data.length) windowSize = data.length;

    const result: number[] = new Array(data.length);
    const halfWindow = Math.floor(windowSize / 2);

    // Primeiro valor
    let sum = 0;
    const firstEnd = Math.min(halfWindow + 1, data.length);
    for (let j = 0; j < firstEnd; j++) {
        sum += data[j];
    }
    result[0] = sum / firstEnd;

    // Sliding window
    for (let i = 1; i < data.length; i++) {
        // Sliding window update

        // Atualizar soma removendo valor antigo e adicionando novo
        if (i - halfWindow - 1 >= 0) {
            sum -= data[i - halfWindow - 1];
        }
        if (i + halfWindow < data.length) {
            sum += data[i + halfWindow];
        }

        const windowLen = Math.min(i + halfWindow, data.length - 1) - Math.max(0, i - halfWindow) + 1;
        result[i] = sum / windowLen;
    }

    return result;
}

// ============================================
// DETECÇÃO E CORREÇÃO DE ARTEFATOS
// ============================================

/**
 * Detecta segmentos com artefatos de movimento
 * Retorna índices de início e fim dos segmentos problemáticos
 */
export function detectArtifacts(signal: number[], sampleRate: number): Array<{ start: number, end: number }> {
    const artifacts: Array<{ start: number, end: number }> = [];

    // Calcular amplitude local
    const windowSize = Math.floor(sampleRate * 0.5); // 500ms
    const amplitudes: number[] = [];

    for (let i = 0; i < signal.length; i += windowSize) {
        const end = Math.min(i + windowSize, signal.length);
        const segment = signal.slice(i, end);
        const amplitude = Math.max(...segment) - Math.min(...segment);
        amplitudes.push(amplitude);
    }

    // Calcular estatísticas
    const sortedAmps = [...amplitudes].sort((a, b) => a - b);
    const medianAmp = sortedAmps[Math.floor(sortedAmps.length / 2)];

    // Threshold: amplitude muito diferente da mediana
    const threshold = medianAmp * 2.5;

    // Encontrar segmentos problemáticos
    let inArtifact = false;
    let artifactStart = 0;

    for (let i = 0; i < amplitudes.length; i++) {
        const isArtifact = amplitudes[i] < medianAmp * 0.3 || amplitudes[i] > threshold;

        if (isArtifact && !inArtifact) {
            artifactStart = i * windowSize;
            inArtifact = true;
        } else if (!isArtifact && inArtifact) {
            artifacts.push({
                start: artifactStart,
                end: i * windowSize
            });
            inArtifact = false;
        }
    }

    // Fechar último artefato se necessário
    if (inArtifact) {
        artifacts.push({
            start: artifactStart,
            end: signal.length
        });
    }

    return artifacts;
}

/**
 * Interpola segmentos com artefatos (para sinal visual)
 * Não usar para cálculo de HRV - apenas descarta esses segmentos
 */
export function interpolateArtifacts(
    signal: number[],
    artifacts: Array<{ start: number, end: number }>
): number[] {
    const result = [...signal];

    for (const { start, end } of artifacts) {
        if (start <= 0 || end >= signal.length) continue;

        const startVal = signal[start - 1];
        const endVal = signal[Math.min(end, signal.length - 1)];
        const length = end - start;

        // Interpolação linear
        for (let i = 0; i < length; i++) {
            result[start + i] = startVal + (endVal - startVal) * (i / length);
        }
    }

    return result;
}

// ============================================
// PIPELINE COMPLETO DE PRÉ-PROCESSAMENTO
// ============================================

export interface PreprocessingResult {
    /** Sinal limpo pronto para detecção de picos */
    cleanSignal: number[];
    /** Sinal original normalizado (para visualização) */
    normalizedSignal: number[];
    /** Segmentos com artefatos detectados */
    artifacts: Array<{ start: number, end: number }>;
    /** Qualidade do sinal (0-100) */
    signalQuality: number;
}

/**
 * Pipeline completo de pré-processamento PPG
 * 
 * Etapas:
 * 1. Normalização (0-1)
 * 2. Remoção de baseline com wavelet
 * 3. Wavelet denoising
 * 4. Detecção de artefatos
 */
export function preprocessPPG(signal: number[], sampleRate: number): PreprocessingResult {
    if (!signal || signal.length < 100) {
        return {
            cleanSignal: signal || [],
            normalizedSignal: signal || [],
            artifacts: [],
            signalQuality: 0
        };
    }

    // 1. Normalização
    const min = Math.min(...signal);
    const max = Math.max(...signal);
    const range = max - min || 1;
    const normalized = signal.map(v => (v - min) / range);

    // 2. Remoção de baseline (respiração, drift)
    const noBaseline = removeBaselineWavelet(normalized, sampleRate);

    // 3. Wavelet denoising (ruído de alta frequência)
    const denoised = waveletDenoise(noBaseline, 4);

    // 4. Normalizar resultado final
    const denoisedMin = Math.min(...denoised);
    const denoisedMax = Math.max(...denoised);
    const denoisedRange = denoisedMax - denoisedMin || 1;
    const cleanSignal = denoised.map(v => (v - denoisedMin) / denoisedRange);

    // 5. Detectar artefatos restantes
    const artifacts = detectArtifacts(cleanSignal, sampleRate);

    // 6. Calcular qualidade do sinal
    const artifactSamples = artifacts.reduce((sum, a) => sum + (a.end - a.start), 0);
    const cleanRatio = 1 - (artifactSamples / signal.length);
    const signalQuality = Math.round(cleanRatio * 100);

    return {
        cleanSignal,
        normalizedSignal: normalized,
        artifacts,
        signalQuality
    };
}

/**
 * Versão simplificada para processamento rápido
 * Usa apenas bandpass filter sem wavelet
 */
export function preprocessPPGFast(signal: number[], sampleRate: number): number[] {
    if (!signal || signal.length < 100) return signal || [];

    // Normalização
    const min = Math.min(...signal);
    const max = Math.max(...signal);
    const normalized = signal.map(v => (v - min) / (max - min || 1));

    // Filtro passa-banda
    const filtered = bandpassFilter(normalized, sampleRate);

    // Normalizar saída
    const fMin = Math.min(...filtered);
    const fMax = Math.max(...filtered);
    return filtered.map(v => (v - fMin) / (fMax - fMin || 1));
}
