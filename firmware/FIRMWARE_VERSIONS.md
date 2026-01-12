# Registro de Versões do Firmware

Este documento rastreia as versões estáveis do firmware para evitar regressões e garantir que sempre haja um ponto de retorno seguro.

| Versão | Arquivo de Backup | Data | Status | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| **v6.1** | `PulseAnalytics_v6.1_Stable_WiFiHome.txt` | 11/01/2026 | **ESTÁVEL** | Reversão para Wi-Fi Freitas apenas. Comandos Serial para USER/AGE/SEX/TAG. 120s Streaming. |
| **v6.0** | `PulseAnalytics_v6_DS_FieldReady_Backup.txt` | 11/01/2026 | *Instável* | Tentativa de BLE + WiFiMulti. Apresentou travamentos e erro de conexão. |
| **v5.0** | `dispositivo_v5.txt` | Anterior | Antiga | Base com SPIFFS e log local. |

## Ponto de Restauração Atual: v6.1
Para voltar ao estado estável atual, basta copiar o conteúdo de `firmware/legacy_versions/PulseAnalytics_v6.1_Stable_WiFiHome.txt` para o arquivo principal no Arduino IDE.

> [!IMPORTANT]
> A partir de agora, **nenhuma** alteração experimental será feita sem antes gerar um arquivo `.txt` de backup nesta pasta.
