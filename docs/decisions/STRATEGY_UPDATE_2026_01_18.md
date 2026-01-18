# Atualização de Estratégia e Decisões - 18/01/2026

Este documento resume as discussões e decisões técnicas tomadas para a evolução do projeto Pulse Analytics, focado na coleta de dados para treinamento de modelos de IA e implementação de funcionalidades "standalone".

## 1. Estratégia de Machine Learning e Processamento de Sinais

### Pipeline Definido
Decidimos manter a alta taxa de amostragem na coleta para preservar a qualidade dos dados e realizar o downsampling via software para compatibilidade com modelos pré-existentes.

1.  **Coleta (Edge):** ESP32 com firmware `v15_optimal` rodando a **757 Hz**.
2.  **Armazenamento:** Supabase (raw waveforms).
3.  **Processamento Offline (Python):**
    *   **Decomposição Wavelet:** Para reduzir ruído e ressaltar características.
    *   **Decimação:** Downsampling inteligente de 757 Hz para **125 Hz**.
4.  **Treinamento (MIMIC-II):**
    *   Utilizar dataset **MIMIC-II** (nativo 125 Hz) para treinar arquitetura **Performer**.
    *   Utilizar dados próprios decimados (125 Hz) para fine-tuning.
5.  **Inferência (Real-time):**
    *   **Transfer Learning:** Adaptar o modelo treinado para rodar a **200 Hz** (ou manter 125 Hz decimado no edge).
    *   **Deploy:** ESP32 (TensorFlow Lite Micro) ou Edge Function.

### Por que 125 Hz?
*   **Compatibilidade:** Dataset MIMIC-II é o "padrão ouro" e está em 125 Hz.
*   **Averaging vs Decimação:**
    *   `1000 Hz / 8 sampleAverage = 125 Hz`: Reduz ruído na fonte, mas menor resolução temporal real.
    *   `757 Hz sampleAverage=1 + Decimação Software`: Preserva morfologia original, permite filtragem digital avançada (Wavelet) antes de reduzir a taxa.
    *   **Decisão:** Coletar com máxima qualidade (757 Hz) e decimar offline.

## 2. Coleta de Dados para Treinamento

### Protocolo de Coleta
*   **Firmware:** `v15_optimal` (757 Hz).
*   **Duração:** **2 a 3 minutos** por sessão (suficiente para treinamento de detecção de picos e morfologia).
*   **Variabilidade:** Focar em **múltiplas sessões curtas** em condições variadas ao invés de poucas sessões longas.
    *   Tags sugeridas: `repouso`, `pos-cafe`, `pos-exercicio`, `estresse`, `manha`, `noite`.

## 3. Arquitetura do Sistema e App

### Conectividade e "Standalone"
O objetivo é reduzir a dependência do PC para coletas diárias.

*   **Web Serial API (Atual):**
    *   ✅ Funciona via USB no Chrome (PC/Android).
    *   ✅ Deploy na Vercel (HTTPS) funciona nativamente.
    *   ❌ Requer cabo físico.
*   **Web Bluetooth API (Futuro):**
    *   Permitiria conexão sem fio direto do navegador (Chrome) para o ESP32.
    *   **Obs:** Evitar por enquanto devido a histórico de conflitos de memória (WiFi + BLE) no ESP32.
*   **Modo Standalone (Imediato):**
    *   Utilizar o **Botão BOOT** da placa ESP32 para iniciar medição.
    *   Fluxo: Ligar na bateria -> Aguardar WiFi -> Pressionar BOOT (1s) -> Coleta -> Upload automático.
    *   App serve apenas para **visualização** posterior.

### Banco de Dados (Supabase)
*   **Estrutura Atual:** Tabela única `hrv_sessions` (legado).
*   **Estrutura Alvo (Futuro):** Separação para otimização.
    *   `sessions`: Metadados, id, user, tags.
    *   `hrv_data`: Métricas leves (RR intervals, SDNN).
    *   `morphology_data`: Dados pesados (Waveforms 757Hz).
*   **Ação:** Manter estrutura atual para coleta imediata. Migração será feita posteriormente.

---

## Próximos Passos (Action Items)

1.  [x] Definir estratégia de ML.
2.  [ ] **Firmware v15:** Adicionar acionamento via botão BOOT (GPIO 0).
3.  [ ] **Coleta:** Iniciar campanha de coleta de dados (v15, 2min, tags variadas).
4.  [ ] **Backend:** Migrar schema do banco de dados (Low priority).
