# 📋 Próximo Plano - 27/01/2026

## Status Atual (27/01/2026 - 15:44)

### ✅ Concluído Hoje
- [x] Verificar treinamento ML (rodando +30 épocas)
- [x] Criar firmware v17_1khz com PSRAM

### 🔄 Em Andamento
- **Modelo**: Performer treinando mais 30 épocas
- **Firmware**: v17_1khz pronto para teste

---

## Próximos Passos

### 1. ⚡ Testar Firmware v17 (AGORA)
- [ ] Abrir Arduino IDE
- [ ] Carregar `firmware/PulseAnalytics_v17_1khz/PulseAnalytics_v17_1khz.ino`
- [ ] Compilar e fazer upload para ESP32-S3
- [ ] Verificar no Serial Monitor:
  - PSRAM encontrada? (deve mostrar ~8MB)
  - Taxa real atingida? (target: 1000 Hz)
- [ ] Fazer 2-3 coletas de teste (50s cada)
- [ ] Verificar upload para Supabase

### 2. 📊 Comparar Qualidade 1kHz vs 757Hz
Após coletas, analisar no notebook:
```python
# Comparar sessões v17 (1kHz) vs v15.2 (757Hz)
# - SNR (Signal-to-Noise Ratio)
# - Detecção de picos
# - Clareza da forma de onda
```

### 3. 🧠 Verificar Treinamento ML
- [ ] Verificar se completou 50+30 = 80 épocas
- [ ] Analisar F1-score final
- [ ] Testar inferência nos dados ESP32

### 4. 📦 Commit Final
```bash
git add .
git commit -m "feat: Add v17 firmware with PSRAM 1kHz sampling"
```

---

## Configurações v17

| Parâmetro | Valor | Nota |
|-----------|-------|------|
| Sample Rate | 1000 Hz | Máximo do sensor |
| Pulse Width | 118µs | 16-bit ADC (necessário para 1kHz) |
| Buffer Size | 60,000 | ~60s @ 1000Hz |
| Memória | PSRAM | 8MB disponível |
| Duração | 50s | Validação antes de 5 min |

---

## Arquivos Importantes

| Arquivo | Descrição |
|---------|-----------|
| `firmware/PulseAnalytics_v17_1khz/` | **NOVO** - Firmware 1kHz PSRAM |
| `firmware/PulseAnalytics_v15.2_reliable/` | Backup 757Hz (não alterado) |
| `analytics/02_performer_peak_detection.ipynb` | Notebook de treinamento |
| `firmware/FIRMWARE_VERSIONS.md` | Registro de versões |

---

## Decisão de Arquitetura (Mantida)

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32-S3                              │
├─────────────────────────────────────────────────────────────┤
│  1. Coleta PPG @ 1000 Hz (IR + Red)                         │
│  2. Armazena na PSRAM (~60k amostras)                       │
│  3. Faz upload para Supabase                                │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼ WiFi
┌─────────────────────────────────────────────────────────────┐
│                     Supabase (Nuvem)                         │
├─────────────────────────────────────────────────────────────┤
│  1. Recebe dados brutos (preservados para futuro)           │
│  2. Modelo Performer detecta picos                          │
│  3. Calcula RR intervals                                    │
│  4. Análise HRV (SDNN, RMSSD, LF, HF, LF/HF)               │
└─────────────────────────────────────────────────────────────┘
```
