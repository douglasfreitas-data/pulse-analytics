# Roadmap do Projeto Pulse Analytics

> Estratégia de desenvolvimento para detecção de picos PPG e rPPG

## 📋 Visão Geral

O projeto visa desenvolver um sistema robusto de detecção de picos cardíacos em sinais fotopletismográficos, começando com PPG de contato (sensor MAX30102) e evoluindo para rPPG baseado em câmera.

---

## 🎯 Fase 1: PPG de Contato (Atual)

### 1.1 Hardware ✅
- [x] ESP32-S3 + MAX30102
- [x] Firmware v18 híbrido (dual-core, RAM + PSRAM)
- [x] Upload para Supabase

### 1.2 Modelo Base ✅
- [x] Arquitetura Performer treinada
- [x] Dataset: MIMIC-II / BIDMC
- [x] Acurácia base: ~74%

### 1.3 Fine-tuning (Em Progresso)
- [ ] Coletar dados próprios com v18
- [ ] Anotar picos com ferramenta semi-automática
- [ ] Fine-tuning do Performer com dados anotados
- [ ] Meta: >90% acurácia em dados próprios

---

## 🎯 Fase 2: Validação e Otimização

### 2.1 Validação Cruzada
- [ ] Testar modelo em diferentes condições
- [ ] Comparar com oxímetro comercial
- [ ] Documentar limitações

### 2.2 Otimização
- [ ] Aumentar duração das coletas (PSRAM_SIZE)
- [ ] Testar diferentes frequências
- [ ] Otimizar consumo de energia

---

## 🎯 Fase 3: Transfer Learning para rPPG

### 3.1 Estratégia
```
PPG de contato (ground truth)
         ↓
Coleta simultânea PPG + Câmera
         ↓
Modelo rPPG usa picos do cPPG como labels
         ↓
Transfer learning do Performer
```

### 3.2 Implementação
- [ ] Sistema de coleta sincronizada (ESP32 + câmera)
- [ ] Alinhamento temporal dos sinais
- [ ] Fine-tuning do modelo para rPPG
- [ ] Validação com ground truth PPG

---

## 📊 Métricas de Sucesso

| Fase | Métrica | Meta |
|------|---------|------|
| 1 | Acurácia detecção picos | >90% |
| 1 | Correlação HR (PPG vs referência) | >0.95 |
| 2 | RMSE HR | <3 BPM |
| 3 | Acurácia rPPG | >85% |

---

## 🔬 Justificativa Científica

### Por que PPG primeiro?
1. **Sinal mais limpo** - Maior SNR que rPPG
2. **Ground truth confiável** - Picos bem definidos
3. **Transfer learning** - Modelo aprende padrões fundamentais

### Por que Transfer Learning?
1. **Economia de dados** - Menos anotações manuais necessárias
2. **Conhecimento prévio** - Modelo já entende morfologia cardíaca
3. **Adaptação** - Ajuste fino para características do rPPG

---

## 📁 Estrutura do Projeto

```
pulse-analytics/
├── firmware/                    # ESP32 firmware
│   ├── PulseAnalytics_v18_hybrid/  # Versão atual
│   └── ...
├── analytics/                   # Notebooks de análise
│   ├── 01_peak_detection_training.ipynb
│   ├── 02_performer_peak_detection.ipynb
│   ├── 03_peak_annotation.ipynb     # Anotação manual
│   └── tratamento.ipynb
├── docs/                        # Documentação
│   ├── FIRMWARE_DEVELOPMENT_HISTORY.md
│   └── PROJECT_ROADMAP.md          # Este arquivo
└── web_app/                     # Aplicação web
```

---

## 📝 Notas de Desenvolvimento

### Descobertas Importantes
1. **Latência PSRAM**: Causa perda de dados em coleta rápida → Solução: dual-core
2. **Inversão do sinal**: Sensor mede luz transmitida, não absorvida
3. **Posição do dedo**: Indicador funciona melhor no clipe modificado

### Configurações Otimizadas
- **Taxa de amostragem**: 800Hz (real ~757Hz)
- **Pulse width**: 215µs (18-bit ADC)
- **Ring buffer**: 8000 samples (RAM)
- **PSRAM buffer**: 50000 samples (~60s)

---

*Última atualização: 2026-01-31*
