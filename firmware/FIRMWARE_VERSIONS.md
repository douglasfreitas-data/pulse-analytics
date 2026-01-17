# Registro de Versões do Firmware

Este documento rastreia as versões estáveis do firmware para evitar regressões e garantir que sempre haja um ponto de retorno seguro.

| Versão | Arquivo de Backup | Data | Status | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| **v11.0** | `PulseAnalytics_v11_800Hz` | 16/01/2026 | **EXPERIMENTAL** | 800Hz Tanker Mode. PulseWidth 215us. IR Gain 0x7F. Buffer 192KB. |
| **v10.0** | `legacy_versions/PulseAnalytics_v10_GOLD_BACKUP.txt` | 16/01/2026 | **GOLD STANDARD** | Estável @ 400Hz. Tanker Mode (RAM 60s). IR ajustado (0x70) para Indicador. |
| **v9.0** | `legacy_versions/PulseAnalytics_v9_tanker` | 16/01/2026 | *Legado* | Tentativa de 800Hz e 12 batches. Sofreu com buffer overflow no streaming/sensor. |
| **v8.0** | `legacy_versions/PulseAnalytics_v8_cloud` | 14/01/2026 | *Legado* | Base da v10. Sofria com IR saturado (clipping) no dedo indicador. |
| **v7.1** | `PulseAnalytics_v7_green.ino` | 13/01/2026 | *Obsoleto* | RAW Data Collector. Green LED @ 400Hz. Sensor incorreto (era MAX30102, não MAX30105). |
| **v6.1** | `legacy_versions/PulseAnalytics_v6.1_Stable_WiFiHome.txt` | 11/01/2026 | *Backup* | WiFi Freitas apenas. Streaming 120s. |
| **v6.0** | `legacy_versions/PulseAnalytics_v6_DS_FieldReady_Backup.txt` | 11/01/2026 | *Instável* | Tentativa de BLE + WiFiMulti. |
| **v5.0** | `legacy_versions/dispositivo_v5.txt` | Anterior | Antiga | Base com SPIFFS e log local. |

## Ponto de Restauração Atual: v10.0
A versão oficial é `firmware/PulseAnalytics_v10_stable`. Use esta para coletas.
Para voltar ao estado estável atual, basta copiar o conteúdo de `firmware/legacy_versions/PulseAnalytics_v6.1_Stable_WiFiHome.txt` para o arquivo principal no Arduino IDE.

> [!IMPORTANT]
> A partir de agora, **nenhuma** alteração experimental será feita sem antes gerar um arquivo `.txt` de backup nesta pasta.
