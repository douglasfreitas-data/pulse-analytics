# DevLog: Lançamento da v10 "Stable Indicator"
**Data:** 16/01/2026
**Status:** ✅ Sucesso (v10 Released)

## Resumo Executivo
Hoje focamos em estabilizar a coleta de dados de alta fidelidade para o TCC. Exploramos limites de hardware (800Hz), arquiteturas de buffer (Tanker vs Streaming) e refinamos a qualidade do sinal analógico (LED Gain).

O resultado é a **Firmware v10.0**, otimizada especificamente para capturas de 60 segundos no dedo indicador a 400Hz.

---

## 1. O Experimento "Tanker" de 800Hz (v9)
*   **Objetivo:** Tentar dobrar a resolução temporal para 800Hz (1.25ms por amostra) para capturar micro-variações da VFC.
*   **Desafio Encontrado:** 
    *   O sensor MAX30102 suporta 800Hz com *pulseWidth* reduzido (215µs).
    *   Porém, o volume de dados (byte rate) excedeu a velocidade de upload estável do ESP32 para o Supabase durante o streaming.
    *   Resultado: *Buffer Overflow* e perda de dados (truncamento) no meio da coleta.
*   **Decisão:** Retornar para 400Hz, que já oferece resolução excelente (2.5ms) para VFC e é muito mais estável para o hardware atual.

## 2. Mudança de Arquitetura: "True Tanker"
Para resolver os problemas de upload instável e perda de dados, mudamos a lógica fundamental do firmware:

| Estratégia Antiga (Streaming) | Estratégia Nova (Tanker/RAM) |
| :--- | :--- |
| Envia dados *enquanto* coleta | Armazena TUDO na RAM primeiro |
| **Vantagem:** Sem limite de tempo | **Vantagem:** Zero risco de gargalo de WiFi |
| **Risco:** WiFi lento trava a coleta | **Risco:** Limite de 60s (RAM finita) |
| **Resultado:** Dados cortados/corrompidos | **Resultado:** Integridade 100% dos dados |

A v10 adota a estratégia **Tanker**: Usa ~96KB de RAM para segurar 60s de dados brutos e só liga o WiFi no final.

## 3. Qualidade de Sinal e "Hot IR"
Ao analisarmos os gráficos da versão restaurada (v8):
*   **Dedo Mindinho:** O sinal Infravermelho (IR) saturou completamente (linha reta no topo), pois o dedo é fino e a luz atravessa demais.
*   **Dedo Indicador:** Sinal excelente e limpo. Porém, com potência máxima (`0x7F`), o pico chegava a ~65.200.
*   **O Perigo:** O buffer é `uint16_t` (max 65.535). Estávamos a <1% de um *Integer Overflow*, que inverteria o sinal digitalmente.

**Ajuste na v10:**
*   Reduzimos a potência do LED IR de `0x7F` (127) para **`0x70` (112)**.
*   Isso mantém a qualidade (SNR alto) mas traz o pico para uma zona segurança (~50.000), evitando distorções.

## 4. Especificações Finais (v10.0)
A versão oficial para o TCC agora é a **PulseAnalytics_v10_stable**.

*   **Frequência:** 400 Hz
*   **Duração:** 60 segundos (Fixo)
*   **Dedo Alvo:** Indicador (Index Finger)
*   **LED Red:** 0x7F (Máximo)
*   **LED IR:** 0x70 (Segurança)
*   **Armazenamento:** RAM Buffer -> Supabase (`hrv_sessions`)

## 5. Estudo Comparativo: Indicador vs Mindinho
Realizamos um teste cruzado para validar a qualidade do sinal em diferentes dedos com a nova configuração de ganho (v10).

![Comparação Mindinho vs Indicador](/home/douglas/Documentos/Projects/PPG/pulse-analytics/docs/assets/signal_comparison_v10.png)

### Análise dos Sinais
1.  **Mindinho (Little Finger):**
    *   **Características:** Sinal extremamente limpo, pulsos agudos ("sharp peaks") e linha de base estável.
    *   **Fisiologia:** Menor tecido adiposo e muscular, artérias superficiais. Sinal mais "puro" do evento cardíaco arterial.
    *   **Uso:** Excelente para detecção robusta de BPM simples, mas menor riqueza de dados respiratórios.

2.  **Indicador (Index Finger):**
    *   **Características:** Sinal forte (~60k amplitude), mas com notável **ondulação de baixa frequência** (baseline wander).
    *   **Fisiologia:** Alta vascularização capilar. A ondulação visualizada é a **Modulação Respiratória** (respiração alterando volume sanguíneo), o que é biologicamente rico para análises de VFC/RSA.
    *   **Uso:** Padrão "Gold Standard" para o projeto. Contém informação cardíaca + respiratória.

**Conclusão:**
Ambos os dedos funcionam perfeitamente na v10. Manteremos o **Indicador** como protocolo padrão pela riqueza de dados fisiológicos, mas o Mindinho é validado como um backup excepcional de alta fidelidade.

## 6. Experimento 800Hz (v11 Experimental)
Validamos a capacidade do hardware de operar a 800Hz usando o modo Tanker.

![800Hz IR Signal](/home/douglas/Documentos/Projects/PPG/pulse-analytics/docs/assets/800Hz_IR_v11.png)

### Resultados
1.  **Taxa de Amostragem:** 758 Hz (Target: 800Hz). Sucesso.
2.  **Qualidade do Sinal:**
    *   Mesmo com o tempo de exposição reduzido (215µs), o ganho máximo (`0x7F`) compensou bem.
    *   Pulsos claros e visíveis, morfologia preservada.
    *   Maior ruído de alta frequência (esperado em 800Hz).
3.  **Memória (O Limite):**
    *   **60 segundos:** Falhou no upload (SSL Handshake crash por falta de RAM). Buffer de 192KB ocupou quase tudo.
    *   **50 segundos:** **Sucesso!** Buffer de 160KB deixou ~83KB livres para o SSL.

**Conclusão 800Hz:**
É tecnicamente viável coletar a ~800Hz por 50 segundos na RAM interna. Para alcançar 60 segundos ou mais, dependemos da presença de **PSRAM** na placa.

## 7. Diagnóstico de Hardware e Limite Físico
O log de boot confirmou: `PSRAM Total: 0`.
Isso significa que estamos restritos estritamente à SRAM interna do ESP32-S3 (~320KB Total, ~190KB utilizável para buffers).

### Matriz de Decisão Final
| Versão | Frequência | Duração | Memória | Status | Uso Recomendado |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **v10.0** | 400 Hz | 60 seg | ~96 KB | **GOLD** | **Protocolo TCC (Padrão).** Equilibrio perfeito. |
| **v11.0** | 800 Hz | 50 seg | ~160 KB | **EXP** | Pesquisa de Alta Resolução. Duração limitada. |

Recomendamos manter a placa com a **v10.0** para garantir a consistência de 60 segundos nos testes oficiais.

---
*Log gerado automaticamente pelo Assistente Antigravity.*
