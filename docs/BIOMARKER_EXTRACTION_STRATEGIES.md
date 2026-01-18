# 🧬 Estratégias para Extração de Biomarcadores de PPG 800Hz

> **Objetivo:** Maximizar a extração de informações clínicas de sinais PPG de alta resolução usando técnicas de processamento de sinal e deep learning.

---

## 📋 Índice

1. [Contexto e Dados Disponíveis](#contexto)
2. [Biomarcadores Alvo](#biomarcadores)
3. [Estratégias de Extração](#estrategias)
4. [Arquitetura Proposta](#arquitetura)
5. [Pipeline de Treinamento](#pipeline)
6. [Próximos Passos](#proximos)

---

## 1. Contexto e Dados Disponíveis {#contexto}

### Hardware
- **Sensor:** MAX30102 (IR + Red)
- **Taxa real:** ~757 Hz
- **Resolução ADC:** 17-bit (pulseWidth 215μs)
- **Configuração otimizada:** LED 0x90, IR 0x70

### Dados Coletados (Próprios)
- **Supabase:** Sessões de 50s com waveforms IR e Red brutas
- **Taxa real:** ~757 Hz
- **Firmware:** v15 Optimal

### Datasets Externos Disponíveis

#### MIMIC II (BIDMC) - PhysioNet
| Campo | Valor |
|-------|-------|
| **Pacientes** | 53 sujeitos |
| **Taxa** | **125 Hz** |
| **Sinais** | RESP, **PLETH (PPG)**, V, AVR, **II (ECG Lead II)** |
| **Formato** | CSV com timestamp |
| **Duração** | ~8 minutos por paciente |

```csv
# Estrutura do arquivo *_Signals.csv:
Time [s], RESP, PLETH, V, AVR, II
0,       0.35,  0.43,  0.52, 0.30, 0.72
0.008,   0.35,  0.43,  0.51, 0.33, 0.67
```
- **PLETH** = PPG (fotopletismografia)
- **II** = ECG Lead II (padrão para detectar picos R)

#### WESAD - Wearable Stress and Affect Detection
- Dataset de estresse com PPG + outros sensores
- Disponível para validação complementar

### Vantagem da Alta Taxa (Própria vs Dataset)
| Fonte | Taxa | Resolução Temporal | Uso |
|-------|------|---------------------|-----|
| MIMIC II | 125 Hz | 8 ms | Treinamento (ground truth ECG) |
| Wearables | 25 Hz | 40 ms | Comparação |
| **Próprio (v15)** | **757 Hz** | **1.32 ms** | Morfologia fina, dicrótico notch |

---

## 2. Biomarcadores Alvo {#biomarcadores}

### Grupo A: HRV (Variabilidade)
| Métrica | Domínio | Significado Clínico |
|---------|---------|---------------------|
| SDNN | Tempo | Resiliência autonômica geral |
| RMSSD | Tempo | Tônus parassimpático (vagal) |
| pNN50 | Tempo | Saúde vagal |
| LF (0.04-0.15 Hz) | Frequência | Regulação barorreflexa |
| HF (0.15-0.40 Hz) | Frequência | Modulação respiratória |
| LF/HF | Frequência | Balanço simpato-vagal |

> ⚠️ **Limitação:** LF requer janelas de pelo menos 2-5 minutos.

### Grupo B: Morfologia Arterial
| Métrica | Cálculo | Significado |
|---------|---------|-------------|
| Dicrotic Notch | 2ª derivada, zero-crossing | Fechamento válvula aórtica |
| RI (Reflexion Index) | A_dias / A_sys | Resistência vascular periférica |
| SI (Stiffness Index) | altura / ΔT_picos | Rigidez arterial |
| LVET | T_sys → T_notch | Tempo ejeção ventricular |

### Grupo C: Envelhecimento Vascular (APG)
| Componente | Descrição |
|------------|-----------|
| Onda 'a' | Aceleração sistólica inicial |
| Onda 'b' | Desaceleração sistólica |
| Onda 'c' | Reaceleração (reflexão) |
| Onda 'd' | Desaceleração diastólica |
| Onda 'e' | Entalhe dicrótico |
| **AGI** | (b - c - d - e) / a |

### Grupo D: Parâmetros Respiratórios
| Métrica | Extração |
|---------|----------|
| Freq. Respiratória | Modulação de envelope (0.1-0.5 Hz) |
| RSA | Variação RR sincronizada com respiração |

---

## 3. Estratégias de Extração {#estrategias}

### Estratégia 1: Processamento Clássico de Sinal

```
PPG Bruto → Bandpass → Detecção de Picos → Intervalos RR → Métricas HRV
         → 1ª Derivada → Pontos fiduciários
         → 2ª Derivada (APG) → Ondas a-e → AGI
         → Envelope → FFT → Freq. Respiratória
```

**Prós:** Interpretável, validado clinicamente
**Contras:** Sensível a ruído, requer tuning manual

### Estratégia 2: Wavelet Multi-Resolution Analysis

```
PPG 800Hz → Wavelet (db4/sym6) 
         → Level 1-4: Ruído de alta frequência
         → Level 5-7: Componente pulsátil (morfologia)
         → Level 8+: Baseline drift e respiração
```

**Decomposição sugerida:**
| Nível | Banda Freq (800Hz base) | Conteúdo |
|-------|-------------------------|----------|
| 1 | 200-400 Hz | Ruído elétrico |
| 2 | 100-200 Hz | Ruído sensor |
| 3 | 50-100 Hz | Alta freq fisiológica |
| 4 | 25-50 Hz | Detalhes de pulso |
| 5 | 12.5-25 Hz | Morfologia principal |
| 6 | 6-12.5 Hz | Envelope do pulso |
| 7 | 3-6 Hz | Taxa cardíaca |
| 8 | 1.5-3 Hz | Variações lentas |
| 9 | 0.75-1.5 Hz | HRV |
| 10 | < 0.75 Hz | Respiração + drift |

### Estratégia 3: Deep Learning End-to-End

#### Opção A: Performer (Attention Eficiente)
```
PPG 800Hz → Patch Embedding → Performer Encoder → Heads específicos
                                                → Head HRV
                                                → Head Morfologia
                                                → Head Respiratório
```

**Por que Performer?**
- FAVOR+ attention: O(n) ao invés de O(n²)
- Pode processar sequências longas (40k amostras)
- Captura dependências de longo alcance

#### Opção B: Encoder-Decoder com ECG como Target
```
PPG (input) → Encoder → Latent → Decoder → ECG (target)
                              → Latent space pode ser usado para features
```

**Vantagem:** ECG é ground truth mais limpo para RR intervals

#### Opção C: Contrastive Learning (Self-Supervised)
```
PPG janela A → Encoder → embedding
PPG janela B → Encoder → embedding
                      → Contrastive Loss (mesmo sujeito = similar)
```

**Vantagem:** Não precisa de labels, aprende representações

### Estratégia 4: Híbrida (Recomendada)

```
PPG 800Hz 
    │
    ├→ Wavelet Denoising (sym6, levels 1-3)
    │
    ├→ Decimação 125Hz (anti-alias filter)
    │
    ├→ Segmentação por batimento (beat-to-beat)
    │
    └→ Performer Multi-Task
         ├→ Task 1: RR intervals (supervisionado com ECG)
         ├→ Task 2: Morfologia (classificação de qualidade)
         ├→ Task 3: Pontos fiduciários (sys, notch, dias)
         └→ Task 4: Biomarcadores agregados (regression)
```

---

## 4. Arquitetura Proposta {#arquitetura}

### Modelo: PPG-Performer

```
┌─────────────────────────────────────────────────────┐
│                    INPUT (125Hz x 5min = 37500)     │
└─────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────┐
│  Patch Embedding (kernel=25 samples = 200ms window) │
│  → 1500 patches de 64 dims                          │
└─────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────┐
│  Positional Encoding (sinusoidal ou learned)        │
└─────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────┐
│  Performer Encoder (6 layers, 4 heads, FAVOR+)      │
│  → Hidden dim: 256                                  │
│  → Random Features: 128                             │
└─────────────────────────────────────────────────────┘
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
┌─────────────────────┐   ┌─────────────────────┐
│  Head: RR Predictor │   │  Head: Biomarkers   │
│  → Per-patch output │   │  → Global pooling   │
│  → Binary: é pico?  │   │  → Regression: SI,  │
│                     │   │    RI, AGI, BPM...  │
└─────────────────────┘   └─────────────────────┘
```

### Loss Functions

```python
# Multi-task loss
L_total = λ1 * L_peaks     # Binary cross-entropy para detecção
        + λ2 * L_rr        # MSE para intervalos RR
        + λ3 * L_morpho    # Triplet loss para embeddings
        + λ4 * L_biomark   # Huber loss para biomarcadores
```

---

## 5. Pipeline de Treinamento: Transfer Learning {#pipeline}

### ⚠️ Problema Chave

> "Não tenho ECG para validar meus dados de 800 Hz"

### ✅ Solução: Transfer Learning com MIMIC II

O ECG do MIMIC **não precisa estar nos seus dados**. Ele é usado apenas para **treinar o modelo** a reconhecer batimentos a partir do PPG. Depois, o modelo aplica esse conhecimento no seu PPG.

```
┌────────────────────────────────────────────────────────────────┐
│                    FASE 1: TREINAMENTO (MIMIC)                 │
│                                                                │
│   PPG (PLETH) 125Hz  ─────────→  MODELO  ←──────── ECG (II)    │
│   [Input]                        aprende           [Ground     │
│                                  a detectar        Truth]      │
│                                  batimentos                    │
└────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────┐
│                    FASE 2: APLICAÇÃO (Seus dados)              │
│                                                                │
│   Seu PPG 757Hz ──→ Decimar 125Hz ──→ MODELO ──→ Picos + RR   │
│                     (ou multi-scale)   pré-     [Predição]     │
│                                        treinado                │
└────────────────────────────────────────────────────────────────┘
```

### Fase 1: Treinamento no MIMIC II

#### 1.1 Pré-processamento do MIMIC
```python
# Carregar um paciente
df = pd.read_csv('bidmc_01_Signals.csv')
ppg = df['PLETH'].values   # PPG a 125 Hz
ecg = df['II'].values      # ECG Lead II a 125 Hz

# Detectar picos R do ECG (ground truth)
r_peaks = detect_ecg_peaks(ecg)  # Pan-Tompkins ou similar

# Criar labels binários para o PPG
# 1 = tem pico R neste timestamp, 0 = não tem
labels = create_binary_labels(len(ppg), r_peaks)
```

#### 1.2 Treinamento Supervisionado
```python
# Input:  PPG segmentado em janelas de 30s
# Output: Probabilidade de cada amostra ser um pico

model = PPGPerformer()
model.train(
    X = ppg_windows,      # Shape: (N, 3750)  # 30s @ 125Hz
    y = peak_labels,      # Shape: (N, 3750)  # Binary
)
```

### Fase 2: Aplicação nos Seus Dados

#### 2.1 Opção A: Decimar para 125 Hz
```python
# Seu PPG 757 Hz → 125 Hz
from scipy.signal import decimate

ppg_800 = load_your_data()           # 757 Hz
ppg_125 = decimate(ppg_800, q=6)     # ~126 Hz (próximo de 125)

# Aplicar modelo pré-treinado
peaks = model.predict(ppg_125)
```

**Prós:** Simples, compatível diretamente  
**Contras:** Perde detalhes de alta frequência

#### 2.2 Opção B: Multi-Scale (Recomendado)
```python
# Manter 757 Hz para morfologia, decimar para detecção de picos

# Branch 1: Detecção de picos (125 Hz)
ppg_125 = decimate(ppg_800, q=6)
peak_indices = model.predict(ppg_125)

# Converter índices de volta para 757 Hz
peak_indices_800 = peak_indices * 6  # Escalar

# Branch 2: Morfologia (757 Hz original)
# Usar os picos detectados para segmentar beat-to-beat
for i in range(len(peak_indices_800) - 1):
    beat = ppg_800[peak_indices_800[i]:peak_indices_800[i+1]]
    # Extrair: notch, RI, SI, APG...
```

**Prós:** Mantém resolução alta para morfologia  
**Contras:** Mais complexo de implementar

### Fase 3: Fine-tuning (Opcional)

Se detectar que os picos estão consistentemente errados nos seus dados:

```python
# Usar pseudo-labels (picos detectados por algoritmo clássico no seu 800 Hz)
pseudo_peaks = detect_peaks_classical(ppg_800)

# Fine-tune o modelo
model.finetune(
    X = decimate(ppg_800, q=6),
    y = pseudo_peaks
)
```

### Fase 4: Validação

| Métrica | Como medir |
|---------|------------|
| **Peak F1** | Quantos picos foram detectados corretamente |
| **RR MAE** | Erro médio dos intervalos em ms |
| **BPM Error** | Diferença de BPM calculado vs referência |

---

## 6. Alternativas à Decimação 800→125 Hz

### Por que decimar?
O MIMIC é 125 Hz. Se treinar o modelo em 125 Hz, ele espera entrada a 125 Hz.

### Alternativas para preservar informação:

| Estratégia | Descrição | Trade-off |
|------------|-----------|-----------|
| **Decimação simples** | 800→125 Hz | Perde detalhes |
| **Multi-scale** | Usa 125 Hz para picos, 800 Hz para morfologia | Mais complexo |
| **Upsample dataset** | 125→800 Hz (interpolação) | Não cria informação real |
| **Treinar do zero** | Só com seus 800 Hz + pseudo-labels | Menos dados |
| **Domain Adaptation** | Treinar 125 Hz, adaptar para 800 Hz | Técnica avançada |

### Recomendação Final

**Multi-Scale + Transfer Learning:**

1. Treinar detector de picos no MIMIC (125 Hz)
2. Aplicar em seus dados decimados para encontrar picos
3. Usar índices dos picos para segmentar o sinal **original** 757 Hz
4. Extrair biomarcadores de morfologia do sinal em alta resolução

---

## 6. Próximos Passos {#proximos}

### Imediato
- [ ] Explorar estrutura dos datasets PPG+ECG disponíveis
- [ ] Definir formato de entrada/saída do modelo
- [ ] Escolher framework (PyTorch + performer-pytorch)

### Curto prazo
- [ ] Implementar pipeline de pré-processamento
- [ ] Treinar modelo baseline (detector de picos simples)
- [ ] Avaliar qualidade dos dados coletados com v15

### Médio prazo
- [ ] Implementar Performer multi-task
- [ ] Treinar com datasets públicos
- [ ] Fine-tune com dados próprios

### Decisões pendentes
1. **Duração das sessões:** 50s (atual) vs 5min (ideal para LF/HF)?
2. **Formato final:** 125Hz fixo ou multi-resolução?
3. **Deploy:** Edge (ESP32) ou Cloud (Supabase Edge Functions)?

---

## 📚 Referências Sugeridas

1. **Performer:** "Rethinking Attention with Performers" (Choromanski et al., 2020)
2. **PPG Morphology:** "Pulse Wave Analysis" (Millasseau et al.)
3. **APG/AGI:** "Aging Index from Photoplethysmography" (Takazawa et al.)
4. **Wavelet PPG:** "Wavelet-based denoising for PPG signals" (Castaneda et al.)

---

*Documento criado em 2026-01-18 | Douglas Freitas & Gemini*
