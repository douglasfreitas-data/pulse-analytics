# Pulse Analytics 🫀

**Sistema IoT de monitoramento cardíaco e análise de variabilidade da frequência cardíaca (HRV) para validação de algoritmos e predição de estados de fadiga/estresse.**

> Projeto de pesquisa e desenvolvimento que combina hardware (ESP32), processamento de sinais e ciência de dados para criar ground truth de métricas cardíacas.

---

## 🎯 Propósito

Estabelecer **"verdade terrestre" (ground truth)** para validação de dispositivos comerciais de oximetria de pulso através de:

- Coleta de sinais PPG de alta precisão (400Hz+)
- Validação estatística de métricas HRV (RMSSD, SDNN, pNN50)
- Desenvolvimento e teste de algoritmos de detecção de picos R
- Análise preditiva de estados de estresse e fadiga

---

## 🔬 Foco: Ciência de Dados

Este projeto prioriza **rigor científico e análise de dados** sobre interface de usuário.

### Pipeline de Dados
```
[ESP32 + MAX30105] → [Coleta 400Hz] → [Filtragem] → [Detecção Picos] 
→ [Cálculo HRV] → [Validação Estatística] → [Ground Truth]
```

### Análises Realizadas

- **Validação contra dispositivo referência** (Polar H10)
- **Análise de acurácia** de algoritmos de detecção
- **Processamento de sinais** (filtros Butterworth, remoção de artefatos)
- **Correlação HRV × Estados fisiológicos**
- **Modelos preditivos** de estresse/fadiga

---

## 🛠️ Stack

### Hardware
- **ESP32** (Dev Module / S3)
- **MAX30105** (sensor PPG de alta resolução)

### Firmware (Coleta)
- **C++** (Arduino/ESP-IDF)
- Taxa de amostragem: 400Hz+
- Transmissão serial de dados brutos

### Data Science (Análise) ⭐
- **Python** | Pandas | NumPy | SciPy | Scikit-learn
- **Jupyter Notebooks** (análise exploratória)
- **Matplotlib/Seaborn** (visualização de sinais)
- **Algoritmos**: Pan-Tompkins, filtros adaptativos

---

## 📊 Datasets

- **10.000+ intervalos RR** coletados e anotados
- **Ground truth** validado contra Polar H10
- **Análise de erro**: MAE < 5ms em detecção de picos
- **Casos de uso**: repouso, exercício, recuperação

---

## 📂 Estrutura
```
pulse-analytics/
├── firmware/           # C++ para ESP32
├── notebooks/          # Análises Python/Jupyter ⭐
├── datasets/           # Dados brutos e processados
├── algorithms/         # Implementação de filtros/detecção
├── docs/              # Metodologia científica
└── README.md
```

---

## 🎯 Aplicação Futura

Integração com **XisPro Analytics** (plataforma de performance esportiva) para monitoramento de fadiga em tempo real.

**Pipeline completo:**
```
Pulse Analytics (validação) → API → XisPro Dashboard → Atletas
```

---

## 🚀 Quick Start

### 1. Firmware (Coleta de Dados)
```bash
cd firmware/
# Abrir no PlatformIO ou Arduino IDE
# Upload para ESP32
```

### 2. Análise de Dados
```bash
cd notebooks/
jupyter notebook
# Abrir: signal_validation.ipynb
```

### 3. Visualizar Dados Brutos
```bash
python scripts/plot_ppg_signal.py --input datasets/session_001.csv
```

---

## 📈 Resultados Preliminares

- **Acurácia de detecção de picos R:** 99.2%
- **Erro médio absoluto (MAE):** 4.3ms
- **Correlação com Polar H10:** r=0.98 (p<0.001)
- **Taxa de amostragem estável:** 412Hz ±3Hz

---

## 🔬 Metodologia

Seguindo protocolo científico de validação de dispositivos HRV:
- Sessões controladas de 5 minutos em repouso
- Comparação simultânea com dispositivo referência
- Análise Bland-Altman para concordância
- Testes estatísticos: correlação de Pearson, t-test pareado

---

## 📚 Referências

- Shaffer, F., & Ginsberg, J. P. (2017). *An Overview of Heart Rate Variability Metrics*
- Pan, J., & Tompkins, W. J. (1985). *A Real-Time QRS Detection Algorithm*
- Task Force (1996). *Heart rate variability: standards of measurement*

---

## 📝 Licença

MIT License

---

## 🤝 Contato

**Douglas Freitas**  
Cientista de Dados | Hardware + Analytics  
📧 douglas.freitas.data@gmail.com  
💼 [linkedin.com/in/douglasfreitas-data](https://linkedin.com/in/douglasfreitas-data)  
💻 [github.com/douglasfreitas-data](https://github.com/douglasfreitas-data)

---

*Parte do ecossistema Pulse para analytics de performance esportiva*
