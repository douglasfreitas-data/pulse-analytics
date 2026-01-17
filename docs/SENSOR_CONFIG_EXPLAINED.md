# 🔧 Configurações do Sensor MAX30102 - PulseAnalytics v11

> Documentação das configurações do sensor para coleta PPG a 800Hz.  
> Baseado nos parâmetros da **Session 18** (configuração que funcionou).

---

## 📋 Visão Geral do Firmware

O firmware `PulseAnalytics_v11_800Hz.ino` tem **755 linhas** com a seguinte estrutura:

| Função | Linhas | Descrição |
|--------|--------|-----------|
| **Configurações** | 1-90 | Buffer, constantes, estados |
| `connectWiFi()` | 91-114 | Conexão WiFi |
| `sendChunk()` | 116-121 | Envio de chunks de dados |
| `uploadRawData()` | 123-330 | Upload principal para Supabase |
| `initSPIFFS()` | 332-364 | Storage local |
| `saveSessionCount()` | 366-372 | Contador de sessões |
| `processCommand()` | 374-449 | Comandos serial |
| `startCollection()` | 451-482 | Inicia coleta |
| `handleCollection()` | 484-573 | Processamento da coleta |
| `handleUploading()` | 575-600 | Gerencia estado de upload |
| `showWaitingScreen()` | 602-622 | Tela OLED |
| `setup()` | 638-725 | Inicialização |
| `loop()` | 727-754 | Loop principal |

---

## 🎛️ Parâmetros do Sensor (Linhas 699-712)

### Configuração Principal

```cpp
byte ledBrightness = 0x7F;  // Brilho do LED (0x00-0xFF) → 127 (50%)
byte sampleAverage = 1;     // Sem média → Essencial para 800Hz real
byte ledMode = 2;           // Modo 2 = Red + IR (sem Green)
int sampleRate = 800;       // Taxa de amostragem desejada
int pulseWidth = 215;       // Largura do pulso em μs
int adcRange = 16384;       // Resolução ADC (14-bit → 16384)

particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
```

### Ajuste Fino das Amplitudes

```cpp
particleSensor.setPulseAmplitudeRed(0x7F);  // Red em 127 (máximo)
particleSensor.setPulseAmplitudeIR(0x70);   // IR em 112 (levemente menor)
```

---

## 📊 Explicação Detalhada de Cada Parâmetro

### 1. `ledBrightness` = `0x7F` (127)

**O que faz:** Controla a intensidade geral dos LEDs.

| Valor | Intensidade |
|-------|-------------|
| `0x00` | LED desligado |
| `0x7F` | 50% (atual) |
| `0xFF` | 100% (máximo) |

**Por que 50%?** Evita saturação do sinal e aquecimento excessivo do sensor.

---

### 2. `sampleAverage` = `1`

**O que faz:** Número de amostras que são calculadas a média antes de disponibilizar.

| Valor | Efeito na Taxa |
|-------|----------------|
| `1` | Taxa completa (800Hz → 800Hz) |
| `2` | Taxa / 2 (800Hz → 400Hz) |
| `4` | Taxa / 4 (800Hz → 200Hz) |
| `8` | Taxa / 8 (800Hz → 100Hz) |

> [!IMPORTANT]
> Para 800Hz real, **DEVE ser 1**. Qualquer outro valor divide a taxa efetiva.

---

### 3. `ledMode` = `2`

**O que faz:** Define quais LEDs estão ativos.

| Modo | LEDs Ativos | Uso |
|------|-------------|-----|
| `1` | Red apenas | SpO2 básico |
| `2` | Red + IR | PPG completo (atual) |
| `3` | Red + IR + Green | MAX30105 apenas |

> [!NOTE]
> O MAX30102 **não tem LED verde**. Modo 3 só funciona no MAX30105.

---

### 4. `sampleRate` = `800`

**O que faz:** Taxa de amostragem em Hz.

**Valores suportados pelo MAX30102:**
- 50, 100, 200, 400, 800, 1000, 1600, 3200 Hz

> [!WARNING]
> A taxa efetiva depende do `pulseWidth`. Nem todas as combinações são válidas.

---

### 5. `pulseWidth` = `215μs`

**O que faz:** Tempo que o LED fica aceso por amostra. Afeta:
- Resolução do ADC
- Taxa máxima possível
- Consumo de energia

---

### 6. `adcRange` = `16384`

**O que faz:** Range do conversor analógico-digital.

| Valor | Bits | Sensibilidade |
|-------|------|---------------|
| 2048 | 11-bit | Muito sensível |
| 4096 | 12-bit | Sensível |
| 8192 | 13-bit | Médio |
| **16384** | **14-bit** | Baixa (atual) |

**Por que 16384?** Maior faixa dinâmica = menos chance de saturação.

---

## ⚡ Relação pulseWidth vs sampleRate (Limite Físico)

Esta é a **tabela mais importante** para entender os limites do sensor:

| pulseWidth | Resolução ADC | Taxa Máxima Permitida |
|------------|---------------|----------------------|
| 69 μs | 15-bit | 3200 Hz |
| 118 μs | 16-bit | 1600 Hz |
| **215 μs** | **17-bit** | **1000 Hz** ✅ |
| 411 μs | 18-bit | 400 Hz |

> [!TIP]
> Com `pulseWidth = 215μs`, a taxa máxima teórica é **1000Hz**.  
> Configurar 800Hz está **dentro do permitido**. ✅

---

## 🎯 Por que IR está em 0x70 e não 0x7F?

```cpp
particleSensor.setPulseAmplitudeRed(0x7F);   // Red: máximo
particleSensor.setPulseAmplitudeIR(0x70);    // IR: 88% do máximo
```

O comentário no código explica:

> *"Na Session 18, o IR em 0x70 foi a 'Bala de Prata' contra a saturação"*

### O Problema da Saturação

Quando o sinal atinge o valor máximo do ADC, os picos são "cortados":

```
Sinal Normal:        Sinal Saturado:
    /\                  ___
   /  \                /   \
  /    \              /     \
 /      \            /       \
```

**Reduzir o IR para 0x70** evita que o sinal bata no "teto", preservando os picos do PPG.

---

## 🔌 Configuração I2C

```cpp
Wire.begin();
Wire.setClock(400000);  // 400kHz Fast Mode
```

> [!IMPORTANT]
> O clock I2C de 400kHz é **essencial** para conseguir ler o FIFO a tempo em taxas altas.

---

## 📦 Buffer de Dados

```cpp
const int BUFFER_SIZE = 40000;
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
```

### Cálculo de Memória

- 40.000 amostras × 2 canais × 2 bytes = **160KB**
- ESP32-S3 tem ~320KB de RAM disponível
- Sobram ~160KB para WiFi/SSL

### Duração da Coleta

- 40.000 amostras ÷ 800Hz = **50 segundos**

---

## 📡 Leitura do FIFO (Linhas 484-512)

O sensor tem um **FIFO interno de 32 amostras**. A 800Hz:
- Nova amostra a cada **1.25ms**
- FIFO enche em **40ms**

```cpp
while (particleSensor.available()) {
    uint32_t irValue = particleSensor.getFIFOIR();
    uint32_t redValue = particleSensor.getFIFORed();
    
    irBuffer[bufferIndex] = (uint16_t)irValue;
    redBuffer[bufferIndex] = (uint16_t)redValue;
    bufferIndex++;
    
    particleSensor.nextSample();
}
particleSensor.check();  // Atualiza dados disponíveis
```

> [!CAUTION]
> Se não ler o FIFO rápido o suficiente, **amostras serão perdidas** (FIFO overflow).

---

## 🔗 Links Úteis

- [MAX30102 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX30102.pdf)
- [SparkFun MAX30105 Library](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library)
- [Firmware v11](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/firmware/PulseAnalytics_v11_800Hz/PulseAnalytics_v11_800Hz.ino)

---

*Documento gerado em 2026-01-17*
