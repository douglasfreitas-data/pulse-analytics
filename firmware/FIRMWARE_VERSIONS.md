# Registro de Versões do Firmware

Este documento rastreia as versões estáveis do firmware para evitar regressões e garantir que sempre haja um ponto de retorno seguro.

---

## 🎯 Versões Atuais (Produção)

| Versão | Pasta | Propósito | Taxa | Duração | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **v17.0** | `PulseAnalytics_v17_1khz` | **ML Dataset** - Coleta máxima resolução | 1000 Hz | 50s | **TESTE** |
| **v16.0** | `PulseAnalytics_v16_HRV` | **"Meu Dia"** - Medição diária HRV | 200 Hz | 5 min | **ATIVO** |
| **v15.2** | `PulseAnalytics_v15.2_reliable` | **"Check-up"** - Biomarcadores/Morfologia | 757 Hz | 50s | **ATIVO** |

### Quando usar cada versão:

- **v17 (1kHz PSRAM)**: Validação de qualidade de sinal a 1000Hz vs 757Hz - EXPERIMENTAL
- **v16 (HRV)**: Uso diário para medição de estresse, recuperação e balanço autonômico
- **v15.2 (Biomarcadores)**: Uso semanal/mensal para análise de saúde vascular (RI, SI, APG)

---

## 📚 Histórico de Versões

| Versão | Arquivo de Backup | Data | Status | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| **v17.0** | `PulseAnalytics_v17_1khz` | 27/01/2026 | **TESTE** | 1000Hz PSRAM Mode. Buffers na PSRAM (60k). pulseWidth=118µs. Validação de qualidade. |
| **v16.0** | `PulseAnalytics_v16_HRV` | 18/01/2026 | **PROD** | HRV Mode "Meu Dia". 200Hz x 5 min. Buffer 60k. |
| **v15.2** | `PulseAnalytics_v15.2_reliable` | 26/01/2026 | **PROD** | Upload confiável. Reconexão WiFi forçada. |
| **v15.0** | `PulseAnalytics_v15_optimal` | 17/01/2026 | **PROD** | Biomarcadores Mode. 757Hz x 50s. Config R08 (Matrix winner). |
| **v14.0** | `PulseAnalytics_v14_test_matrix` | 17/01/2026 | *Teste* | Matriz de testes para encontrar config ótima. |
| **v13.0** | `PulseAnalytics_v13_session18_replica` | 17/01/2026 | *Teste* | Réplica da sessão 18 (melhor sinal até então). |
| **v12.0** | `PulseAnalytics_v12_Recovery` | 17/01/2026 | *Legado* | Versão de recuperação. |
| **v11.0** | `PulseAnalytics_v11_800Hz` | 16/01/2026 | *Legado* | 800Hz Tanker Mode. PulseWidth 215us. IR Gain 0x7F. Buffer 192KB. |
| **v10.0** | `legacy_versions/PulseAnalytics_v10_GOLD_BACKUP.txt` | 16/01/2026 | *Legado* | Estável @ 400Hz. Tanker Mode (RAM 60s). IR ajustado (0x70) para Indicador. |
| **v9.0** | `legacy_versions/PulseAnalytics_v9_tanker` | 16/01/2026 | *Legado* | Tentativa de 800Hz e 12 batches. Sofreu com buffer overflow no streaming/sensor. |
| **v8.0** | `legacy_versions/PulseAnalytics_v8_cloud` | 14/01/2026 | *Legado* | Base da v10. Sofria com IR saturado (clipping) no dedo indicador. |
| **v7.1** | `PulseAnalytics_v7_green.ino` | 13/01/2026 | *Obsoleto* | RAW Data Collector. Green LED @ 400Hz. Sensor incorreto (era MAX30102, não MAX30105). |
| **v6.1** | `legacy_versions/PulseAnalytics_v6.1_Stable_WiFiHome.txt` | 11/01/2026 | *Backup* | WiFi Freitas apenas. Streaming 120s. |
| **v6.0** | `legacy_versions/PulseAnalytics_v6_DS_FieldReady_Backup.txt` | 11/01/2026 | *Instável* | Tentativa de BLE + WiFiMulti. |
| **v5.0** | `legacy_versions/dispositivo_v5.txt` | Anterior | Antiga | Base com SPIFFS e log local. |

---

## Ponto de Restauração Atual: v15.0 / v16.0
A versão oficial é `firmware/PulseAnalytics_v10_stable`. Use esta para coletas.
Para voltar ao estado estável atual, basta copiar o conteúdo de `firmware/legacy_versions/PulseAnalytics_v6.1_Stable_WiFiHome.txt` para o arquivo principal no Arduino IDE.

> [!IMPORTANT]
> A partir de agora, **nenhuma** alteração experimental será feita sem antes gerar um arquivo `.txt` de backup nesta pasta.
