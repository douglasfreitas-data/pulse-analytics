# V3 - DECISÕES DE ARQUITETURA

> **Objetivo:** Documentar as dúvidas levantadas durante o planejamento e como foram resolvidas, demonstrando pensamento crítico sobre viabilidade técnica e econômica do projeto.

---

## 📋 Índice de Decisões

1. [Nomenclatura UX para os Modos de Medição](#nomenclatura)
2. [Organização das Tabelas no Banco](#tabelas)
3. [Estratégia de Treinamento (Decimação + Transfer Learning)](#treinamento)
4. [Priorização do Desenvolvimento](#priorizacao)

---

## 1. Nomenclatura UX para os Modos de Medição {#nomenclatura}

### ❓ Dúvida Original

> "Criar rotina no app de coleta das duas fases, mas não com nome de fase e sim com nomes UX melhores"

### 🤔 Análise

O termo "Fase 1" e "Fase 2" são técnicos e não comunicam valor ao usuário final. Precisamos de nomes que:
- Comuniquem o **benefício** da medição
- Sejam **intuitivos** e não técnicos
- Reflitam a **frequência de uso** esperada

### ✅ Decisão Final

| Interno (Técnico) | Nome UX | Descrição para Usuário | Frequência |
|-------------------|---------|------------------------|------------|
| **Fase 2** (200 Hz) | **"Meu Dia"** | Avalia seu estresse, recuperação e equilíbrio do sistema nervoso | Diária |
| **Fase 1+2** (757 Hz + 200 Hz) | **"Check-up Completo"** | Análise detalhada da saúde vascular + estresse | Semanal/Mensal |

### 💡 Alternativas Consideradas

| Opção | Fase 2 | Fase 1+2 | Motivo de Rejeição |
|-------|--------|----------|---------------------|
| A | "Medição Rápida" | "Medição Completa" | Muito genérico |
| B | "HRV Check" | "Health Check" | Usa sigla técnica |
| C | "Daily Pulse" | "Deep Scan" | Inglês (público BR) |
| **D (escolhido)** | **"Meu Dia"** | **"Check-up Completo"** | Claro, brasileiro, orientado a benefício |

### 📱 Comportamento no App

```
┌─────────────────────────────────────────┐
│           PULSE ANALYTICS               │
├─────────────────────────────────────────┤
│                                         │
│   ┌─────────────────────────────────┐   │
│   │         💚 MEU DIA              │   │
│   │    Estresse & Recuperação       │   │
│   │         ⏱️ 5 minutos            │   │
│   └─────────────────────────────────┘   │
│                                         │
│   ┌─────────────────────────────────┐   │
│   │     🔬 CHECK-UP COMPLETO        │   │
│   │    Saúde Vascular + Estresse    │   │
│   │         ⏱️ 6 minutos            │   │
│   └─────────────────────────────────┘   │
│                                         │
└─────────────────────────────────────────┘
```

---

## 2. Organização das Tabelas no Banco {#tabelas}

### ❓ Dúvida Original

> "Teremos uma nova tabela? Separamos uma para 757Hz e outra para 200Hz? Devemos reorganizar as tabelas?"

### 🤔 Análise

**Opção A: Tabelas Separadas por Frequência**
```sql
-- Tabela para 200 Hz
CREATE TABLE daily_measurements (...);

-- Tabela para 757 Hz
CREATE TABLE checkup_morphology (...);
```

**Prós:**
- Schemas específicos para cada tipo de dado
- Queries mais simples

**Contras:**
- Duplicação de metadados (user, device, tags)
- Dificuldade para relacionar medições do mesmo dia
- Mais complexidade na manutenção

---

**Opção B: Tabela Única com Session Type**
```sql
CREATE TABLE measurements (
    measurement_type TEXT, -- 'daily' ou 'checkup'
    sampling_rate_hz INT,
    -- campos comuns...
);
```

**Prós:**
- Simples
- Histórico unificado

**Contras:**
- Muitos campos NULL (campos de morfologia vazios em 'daily')
- Difícil escalar se os schemas divergirem muito

---

**Opção C: Tabela Base + Tabelas Específicas (escolhida)**
```sql
-- Tabela base com metadados comuns
CREATE TABLE sessions (...);

-- Tabela específica para dados de HRV
CREATE TABLE hrv_data (...);

-- Tabela específica para dados de morfologia
CREATE TABLE morphology_data (...);
```

**Prós:**
- Normalização adequada
- Cada tipo de dado tem seu schema otimizado
- Fácil adicionar novos tipos de medição
- Permite análises cruzadas via session_id

**Contras:**
- JOINs necessários para relatórios completos

### ✅ Decisão Final

**Opção C: Modelo Relacional Normalizado**

```sql
┌────────────────────────────────────────────────────────────────────┐
│                           SESSIONS                                  │
│  (Metadados comuns: user, device, timestamp, tags)                 │
├────────────────────────────────────────────────────────────────────┤
│  id | user_name | created_at | device_id | session_type | tags    │
└────────────────────────────────────────────────────────────────────┘
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
┌─────────────────────┐  ┌─────────────────────┐
│     HRV_DATA        │  │  MORPHOLOGY_DATA    │
│  (session_id FK)    │  │  (session_id FK)    │
├─────────────────────┤  ├─────────────────────┤
│ - sampling_rate     │  │ - sampling_rate     │
│ - duration_sec      │  │ - duration_sec      │
│ - rr_intervals      │  │ - ir_waveform       │
│ - sdnn, rmssd...    │  │ - red_waveform      │
│ - lf, hf, lf_hf     │  │ - ri, si, lvet      │
│                     │  │ - agi, vascular_age │
└─────────────────────┘  └─────────────────────┘
```

### 📊 Schema SQL Proposto

```sql
-- Tabela principal de sessões
CREATE TABLE sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    
    -- Identificação
    device_id TEXT DEFAULT 'ESP32-S3',
    user_name TEXT DEFAULT 'Visitante',
    
    -- Tipo de sessão
    session_type TEXT NOT NULL, -- 'daily' ou 'checkup'
    
    -- Demografia
    user_age SMALLINT,
    user_gender TEXT,
    
    -- Contexto
    tags TEXT[]
);

-- Dados HRV (medição diária)
CREATE TABLE hrv_data (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id UUID REFERENCES sessions(id) ON DELETE CASCADE,
    
    -- Parâmetros de coleta
    sampling_rate_hz INT DEFAULT 200,
    duration_sec INT DEFAULT 300, -- 5 minutos
    
    -- Waveform (opcional, para debug/treinamento)
    ir_waveform JSONB,
    
    -- Intervalos RR
    rr_intervals_ms JSONB,
    rr_count INT,
    
    -- Métricas de tempo
    fc_mean FLOAT,
    sdnn FLOAT,
    rmssd FLOAT,
    pnn50 FLOAT,
    
    -- Métricas de frequência
    lf_power FLOAT,
    hf_power FLOAT,
    lf_hf_ratio FLOAT,
    
    -- Qualidade
    signal_quality FLOAT -- 0-100%
);

-- Dados Morfológicos (check-up completo)
CREATE TABLE morphology_data (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id UUID REFERENCES sessions(id) ON DELETE CASCADE,
    
    -- Parâmetros de coleta
    sampling_rate_hz INT DEFAULT 757,
    duration_sec INT DEFAULT 40,
    
    -- Waveforms alta resolução
    ir_waveform JSONB,
    red_waveform JSONB,
    
    -- Métricas de rigidez arterial
    ri_mean FLOAT, -- Reflection Index
    ri_std FLOAT,
    si_mean FLOAT, -- Stiffness Index
    si_std FLOAT,
    lvet_mean FLOAT, -- Left Ventricular Ejection Time
    
    -- APG (Acceleration Plethysmography)
    agi_mean FLOAT, -- Aging Index
    agi_std FLOAT,
    vascular_age_estimated INT,
    
    -- Qualidade
    signal_quality FLOAT,
    beats_analyzed INT
);
```

### 💡 Benefícios desta Estrutura

1. **Migração suave** - Tabela `hrv_sessions` atual pode ser mapeada para `sessions` + `hrv_data`
2. **Escalabilidade** - Fácil adicionar nova tabela para novos tipos de análise
3. **Queries eficientes** - Cada tabela tem apenas os campos relevantes
4. **Integridade** - FK com CASCADE garante consistência

---

## 3. Estratégia de Treinamento (Decimação + Transfer Learning) {#treinamento}

### ❓ Dúvida Original

> "Podemos decimar de 200Hz para 125Hz para o treinamento com o dataset validado e fazer um transfer learning para os 200Hz depois?"

### 🤔 Análise

Esta é uma abordagem **híbrida** que combina o melhor de dois mundos:

1. **Treino inicial em 125 Hz** → Aproveita o MIMIC com ground truth ECG
2. **Transfer learning para 200 Hz** → Adapta para a frequência de produção

### ✅ Decisão Final: Sim, é a Melhor Estratégia

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PIPELINE DE TREINAMENTO                          │
└─────────────────────────────────────────────────────────────────────┘

ETAPA 1: PRÉ-TREINAMENTO (MIMIC 125 Hz)
┌───────────────────────────────────────────────────────────────────┐
│                                                                   │
│  MIMIC PPG (125 Hz) ──→ Modelo Base ←── MIMIC ECG (Ground Truth) │
│                         (Performer)                               │
│                                                                   │
│  • Aprende: detecção de picos, padrões de batimento              │
│  • Labels: picos R do ECG sincronizados                          │
│  • Loss: Binary CE (é pico / não é pico)                         │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
                              │
                              ▼
ETAPA 2: ADAPTAÇÃO (Transfer Learning)
┌───────────────────────────────────────────────────────────────────┐
│                                                                   │
│  Seus dados PPG (200 Hz) ──→ Modelo Fine-tuned                   │
│                              (pesos do Modelo Base)               │
│                                                                   │
│  • Técnica: Upsample MIMIC para 200 Hz OU                        │
│             Pseudo-labels dos seus dados                          │
│  • Congela: primeiras camadas (features genéricas)               │
│  • Treina: últimas camadas (adaptação de frequência)             │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
                              │
                              ▼
ETAPA 3: VALIDAÇÃO
┌───────────────────────────────────────────────────────────────────┐
│                                                                   │
│  Compare:                                                         │
│  • Picos detectados pelo modelo vs algoritmo clássico            │
│  • RR intervals: MAE < 10ms é aceitável                          │
│  • BPM error: < 2 BPM é excelente                                │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

### 💡 Por que esta estratégia é superior

| Aspecto | Só MIMIC (125 Hz) | Só Seus Dados (200 Hz) | Híbrido (escolhido) |
|---------|-------------------|------------------------|----------------------|
| Ground Truth | ✅ ECG real | ❌ Pseudo-labels | ✅ ECG + pseudo |
| Volume de dados | ✅ 53 pacientes | ⚠️ Poucos inicialmente | ✅ Ambos |
| Frequência final | ❌ 125 Hz | ✅ 200 Hz | ✅ 200 Hz |
| Generalização | ⚠️ Pode não transferir | ⚠️ Overfitting | ✅ Robusto |

### 🔧 Implementação Sugerida

```python
# Etapa 1: Treino no MIMIC
model = PPGPerformer(input_hz=125)
model.train(mimic_ppg, mimic_ecg_labels)
model.save("model_base_125hz.pt")

# Etapa 2: Upsample MIMIC para 200 Hz e continuar treino
mimic_200hz = upsample(mimic_ppg, from_hz=125, to_hz=200)
model_200 = PPGPerformer(input_hz=200)
model_200.load_weights("model_base_125hz.pt", adapt_layers=True)
model_200.finetune(mimic_200hz, mimic_ecg_labels)

# Etapa 3: Fine-tune com seus dados
your_data_200hz = load_your_data()
pseudo_labels = classical_peak_detector(your_data_200hz)
model_200.finetune(your_data_200hz, pseudo_labels, lr=1e-5)
```

---

## 4. Priorização do Desenvolvimento {#priorizacao}

### ❓ Dúvida Original

> "Vamos trabalhar com o desenvolvimento da 'medição diária' primeiro assim já começo a coletar os dados de diferentes pessoas"

### ✅ Decisão: Sim, "Meu Dia" (200 Hz) Primeiro

### 💡 Justificativa

| Critério | "Meu Dia" (200 Hz) | "Check-up Completo" (757 Hz) |
|----------|-------------------|------------------------------|
| **Validação** | Fácil (datasets HRV abundantes) | Difícil (poucos datasets morfológicos) |
| **Demanda de mercado** | Alta (wellness, estresse) | Nicho (cardiologia) |
| **Complexidade técnica** | Menor | Maior (APG, derivadas) |
| **Coleta de dados** | Qualquer pessoa | Precisa mais cuidado |
| **Tempo de medição** | 5 min | 6+ min |

### 📋 Roadmap Priorizado

```
📍 SPRINT 1: Fundação "Meu Dia" (2-3 semanas)
   ├── [ ] Firmware 200 Hz estável (baseado no v15)
   ├── [ ] Novo schema no Supabase (migrations)
   ├── [ ] App: tela "Meu Dia" com Start/Stop
   ├── [ ] Upload de dados via botão no app
   └── [ ] Coleta de dados inicial (família, amigos)

📍 SPRINT 2: Pipeline de Análise (2-3 semanas)
   ├── [ ] Backend: extração de RR intervals
   ├── [ ] Backend: cálculo SDNN, RMSSD, pNN50
   ├── [ ] Backend: análise espectral (LF/HF)
   ├── [ ] App: visualização de resultados
   └── [ ] Validação com dados coletados

📍 SPRINT 3: Modelo de ML (3-4 semanas)
   ├── [ ] Download e pré-processamento do MIMIC
   ├── [ ] Treinamento do Performer em 125 Hz
   ├── [ ] Transfer learning para 200 Hz
   ├── [ ] Validação e fine-tuning
   └── [ ] Deploy do modelo (edge ou cloud)

📍 SPRINT 4: "Check-up Completo" (4-5 semanas)
   ├── [ ] Firmware 757 Hz
   ├── [ ] App: tela "Check-up Completo"
   ├── [ ] Pipeline de morfologia (RI, SI, APG)
   └── [ ] Integração com sessão diária
```

### 🎯 Meta Imediata

**Ao final do Sprint 1:**
- ESP32 coletando dados 200 Hz de forma confiável
- App com botão "Meu Dia" funcionando
- Dados sendo salvos no Supabase com novo schema
- Pronto para coletar dados em campo

---

## 📝 Registro de Decisões (Decision Log)

| Data | Decisão | Justificativa | Impacto |
|------|---------|---------------|---------|
| 2026-01-18 | Treinar em 200 Hz (vs 125 Hz) | Evita overhead de decimação em real-time | Pipeline de produção simplificado |
| 2026-01-18 | Schema normalizado (3 tabelas) | Flexibilidade e escalabilidade | Migração do schema atual necessária |
| 2026-01-18 | Híbrido: MIMIC + Transfer Learning | Combina ground truth com adaptação | 2 etapas de treinamento |
| 2026-01-18 | Priorizar "Meu Dia" | Validação mais fácil, coleta de dados | Check-up Completo adiado para Sprint 4 |
| 2026-01-18 | Nomes UX: "Meu Dia" / "Check-up Completo" | Comunicação clara de benefício | Mudança de nomenclatura no app |

---

## 🏆 Valor para o Portfólio

Este documento demonstra:

1. **Pensamento sistêmico** - Considerar impacto de decisões técnicas em produção
2. **Trade-off analysis** - Avaliar múltiplas opções antes de decidir
3. **Viabilidade econômica** - Escolher soluções que minimizam overhead operacional
4. **UX-first thinking** - Nomear features pelo benefício, não pela técnica
5. **Arquitetura escalável** - Design de banco de dados que crescerá com o projeto
6. **Pragmatismo** - Priorizar o que gera valor mais rápido (coleta de dados)

---

*Documento criado em 2026-01-18 | Douglas Freitas*
