# V2 - DECISÃO DE FREQUÊNCIA PARA TREINAMENTO

---

## 📋 Contexto da Decisão

Temos duas frequências em jogo:

| Fonte | Frequência | Uso |
|-------|------------|-----|
| **MIMIC II Dataset** | 125 Hz | Treinamento (tem ECG ground truth) |
| **Sensor MAX30102** | 200 Hz | Coleta real (Fase 2 - HRV) |

**Pergunta chave:** Em qual frequência treinar o modelo?

---

## ❌ Opção A: Treinar em 125 Hz e Decimar em Real-time

```
┌─────────────────────────────────────────────────────────┐
│  COLETA REAL                                            │
│                                                         │
│  Sensor 200Hz → Decimação → Modelo → Resultado          │
│                 (125Hz)     (125Hz)                     │
│                   ⚠️                                    │
│               OVERHEAD                                  │
└─────────────────────────────────────────────────────────┘
```

### Problemas:
1. **Overhead de processamento** - Decimação a cada batch
2. **Latência adicional** - Filtro anti-alias + downsampling
3. **Complexidade** - Mais um passo no pipeline de inferência
4. **Inconsistência** - Treinou em 125 Hz mas coleta em 200 Hz

### Custo computacional da decimação:
```python
# Cada 5 segundos de dados 200 Hz = 1000 amostras
# Decimação q=1.6 com filtro FIR ordem 30
# Operações por batch: ~30.000 multiplicações
```

---

## ✅ Opção B: Treinar em 200 Hz (RECOMENDADO)

```
┌─────────────────────────────────────────────────────────┐
│  TREINAMENTO                                            │
│                                                         │
│  MIMIC 125Hz → Upsample → Modelo → Treinado em 200Hz    │
│                (200Hz)     (200Hz)                      │
│                  ✓                                      │
│              ONE-TIME                                   │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  COLETA REAL                                            │
│                                                         │
│  Sensor 200Hz → Modelo → Resultado                      │
│                 (200Hz)   (direto!)                     │
│                   ✓                                     │
│             SEM OVERHEAD                                │
└─────────────────────────────────────────────────────────┘
```

### Vantagens:
1. **Zero overhead em inferência** - Entrada direta no modelo
2. **Pipeline simplificado** - Menos código, menos bugs
3. **Latência mínima** - Sensor → Modelo → Resultado
4. **Consistência** - Mesma frequência em treino e produção

### Custo do upsample (apenas no treino):
```python
# MIMIC: 53 pacientes × 8 min = ~424 minutos de dados
# Upsample 125→200 Hz: interpolação linear ou sinc
# Processamento one-time: ~5 segundos total
```

---

## 🎯 Decisão Final

> **Treinar o modelo em 200 Hz**

O upsample do MIMIC (125→200 Hz) é feito **uma única vez** durante o pré-processamento do dataset. Depois disso, o modelo roda nativamente em 200 Hz.

### Trade-off aceito:
- **Upsample do MIMIC** não cria informação "real" de 200 Hz
- Porém, para detecção de picos R, 125 Hz já é suficiente
- O modelo aprenderá features em 200 Hz com a mesma "precisão temporal" de 125 Hz
- Isso é **aceitável** porque:
  - Picos R têm largura de ~100-150 ms (coberto por ambas frequências)
  - Não estamos extraindo morfologia fina na Fase 2

---

## 📊 Comparação de Pipelines

### Pipeline A: Decimação Real-time (rejeitado)

```
Treino:    MIMIC 125Hz ─────────────────→ Modelo 125Hz
                                              │
Produção:  Sensor 200Hz → decimate(q=1.6) → Modelo 125Hz → Resultado
                              ⚠️ overhead
```

**Operações por segundo em produção:** ~6.000 (decimação + modelo)

### Pipeline B: Upsample no Treino (escolhido)

```
Treino:    MIMIC 125Hz → upsample(200Hz) → Modelo 200Hz
                ✓ one-time                    │
Produção:  Sensor 200Hz ─────────────────→ Modelo 200Hz → Resultado
                                   ✓ direto
```

**Operações por segundo em produção:** ~4.000 (só modelo)

---

## 🛠️ Implementação do Upsample

### Método recomendado: Interpolação Sinc (ideal para sinais)

```python
from scipy.signal import resample

def upsample_mimic(ppg_125hz, target_rate=200):
    """
    Upsample MIMIC de 125Hz para 200Hz.
    Ratio: 200/125 = 1.6
    """
    original_samples = len(ppg_125hz)
    target_samples = int(original_samples * (200 / 125))
    
    ppg_200hz = resample(ppg_125hz, target_samples)
    return ppg_200hz
```

### Alternativa: Interpolação Linear (mais simples)

```python
import numpy as np

def upsample_linear(ppg_125hz, target_rate=200):
    original_rate = 125
    ratio = target_rate / original_rate
    
    x_original = np.arange(len(ppg_125hz))
    x_target = np.linspace(0, len(ppg_125hz) - 1, 
                           int(len(ppg_125hz) * ratio))
    
    ppg_200hz = np.interp(x_target, x_original, ppg_125hz)
    return ppg_200hz
```

---

## ⚠️ Considerações Importantes

### O que o upsample NÃO faz:
- ❌ Criar informação que não existe (frequências > 62.5 Hz)
- ❌ Melhorar precisão temporal real
- ❌ Adicionar detalhes morfológicos

### O que o upsample FAZ:
- ✅ Compatibiliza o formato de entrada do modelo
- ✅ Permite treino end-to-end em 200 Hz
- ✅ Simplifica pipeline de produção

### Por que isso é aceitável para HRV:
- Detecção de picos R precisa de ~50-100 ms de resolução
- 125 Hz = 8 ms entre amostras (mais que suficiente)
- 200 Hz = 5 ms entre amostras (margem extra)
- O upsample preserva a informação temporal original

---

## 📈 Para a Fase 1 (Morfologia)

A mesma lógica se aplica, mas com números diferentes:

| Coleta | Dataset (se houver) | Modelo |
|--------|---------------------|--------|
| 757 Hz | MIMIC upsampled? | 757 Hz |

**Nota:** Para morfologia (APG, notch dicrótico), pode ser necessário:
1. Coletar dataset próprio em 757 Hz
2. Ou encontrar datasets de PPG em alta frequência
3. Ou usar self-supervised learning (sem ground truth)

---

## ✅ Resumo Executivo

| Fase | Frequência de Coleta | Frequência de Treino | Pipeline Real-time |
|------|---------------------|----------------------|-------------------|
| **Fase 2 (HRV)** | 200 Hz | 200 Hz | Direto |
| **Fase 1 (Morfologia)** | 757 Hz | 757 Hz | Direto |

**Princípio:** Treinar na mesma frequência que será usada em produção.

---

*Documento criado em 2026-01-18 | Douglas Freitas*
