# 🔧 Troubleshooting: Recuperação de Qualidade do Sinal PPG 800Hz

> **Case Study:** Como resolver um problema de qualidade de sinal quando não há backup disponível.

---

## 📋 Contexto do Problema

### O que aconteceu
Após uma série de atualizações no firmware do sensor MAX30102, a qualidade do sinal PPG degradou significativamente. A **Session 18** era a última sessão com sinal de boa qualidade, mas suas configurações exatas foram perdidas por falta de versionamento adequado.

### Sintomas observados
- Sinal PPG sem picos definidos
- Possível saturação (valores batendo no teto)
- Taxa de amostragem instável
- Uploads falhando por conexão instável

### Lição aprendida
> [!CAUTION]
> **Sempre commitar antes de fazer alterações experimentais!**  
> Este problema poderia ter sido evitado com um simples `git commit` antes de cada experimento.

---

## 🎯 Metodologia de Resolução

### Abordagem: Teste Sistemático

Quando não temos backup da configuração funcional, precisamos **redescobrir** os parâmetros ideais através de testes controlados.

### Variáveis a Testar (Compatíveis com 800Hz)

| Parâmetro | Valores Possíveis | Impacto |
|-----------|-------------------|---------|
| `pulseWidth` | 69, 118, 215 μs | Resolução ADC |
| `adcRange` | 4096, 8192, 16384 | Faixa dinâmica |
| `ledBrightness` | 0x50, 0x7F, 0xFF | Intensidade do LED |
| `irAmplitude` | 0x50, 0x70, 0x7F | Potência do IR |

> [!NOTE]
> Para 800Hz, o `pulseWidth` máximo é **215μs**. Valores maiores (411μs) limitam a taxa a 400Hz.

---

## 🧪 Matriz de Testes Criada

### Firmware v14 - Test Matrix

Criamos uma versão especial do firmware com 12 configurações pré-definidas:

```
Teste 1-3:   pulseWidth 69μs   (menor resolução, mais rápido)
Teste 4-6:   pulseWidth 118μs  (equilíbrio)
Teste 7-12:  pulseWidth 215μs  (máxima resolução para 800Hz)
```

### Configurações Detalhadas

| # | Nome | PW | ADC | LED | IR |
|---|------|-----|------|-----|-----|
| 1 | T01_PW69_ADC4K | 69 | 4096 | 0x7F | 0x7F |
| 2 | T02_PW69_ADC8K | 69 | 8192 | 0x7F | 0x7F |
| 3 | T03_PW69_ADC16K | 69 | 16384 | 0x7F | 0x7F |
| 4 | T04_PW118_ADC4K | 118 | 4096 | 0x7F | 0x7F |
| 5 | T05_PW118_ADC8K | 118 | 8192 | 0x7F | 0x7F |
| 6 | T06_PW118_ADC16K | 118 | 16384 | 0x7F | 0x7F |
| 7 | T07_PW215_ADC4K | 215 | 4096 | 0x7F | 0x7F |
| 8 | T08_PW215_ADC8K | 215 | 8192 | 0x7F | 0x7F |
| 9 | T09_PW215_ADC16K | 215 | 16384 | 0x7F | 0x7F |
| **10** | **T10_Session18_Ref** | **215** | **16384** | **0x7F** | **0x70** |
| 11 | T11_LED_MAX | 215 | 16384 | 0xFF | 0x7F |
| 12 | T12_LED_LOW | 215 | 16384 | 0x50 | 0x50 |

> [!IMPORTANT]
> O **Teste 10** replica a configuração presumida da Session 18 e serve como referência.

---

## 📊 Como Executar os Testes

### 1. Flash do Firmware

```bash
# Compilar e enviar para o ESP32
# Use Arduino IDE ou PlatformIO
```

### 2. Comandos Disponíveis

| Comando | Descrição |
|---------|-----------|
| `t1` a `t12` | Executar teste específico |
| `test1` a `test12` | Mesmo que acima |
| `auto` | Executar todos os 12 testes sequencialmente |
| `configs` | Listar todas as configurações |
| `retry` | Reenviar upload que falhou |
| `help` | Mostrar ajuda |

### 3. Procedimento de Teste

1. Conectar ESP32 via Serial (115200 baud)
2. Digitar `auto` para executar todos os testes
3. Manter o dedo no sensor durante cada coleta (10 segundos)
4. Aguardar upload de cada teste
5. Verificar resultados no Supabase

---

## 📈 Análise dos Resultados

### O que procurar em um bom sinal PPG

```
Sinal BOM:                    Sinal RUIM:
    /\    /\    /\               ___________
   /  \  /  \  /  \             /           
  /    \/    \/    \           /            
                               
- Picos claros                - Sem picos
- Periodicidade visível       - Sinal plano
- Amplitude consistente       - Saturação (teto)
```

### Critérios de Sucesso

- [ ] Picos PPG visíveis e bem definidos
- [ ] Taxa efetiva próxima de 800Hz
- [ ] Sem saturação (valores não batendo em 65535)
- [ ] Amplitude suficiente (não muito baixa)

---

## 📝 Registro de Resultados

Preencha esta tabela após executar os testes:

| Teste | Taxa Real | Amplitude | Picos Visíveis | Saturação | Nota |
|-------|-----------|-----------|----------------|-----------|------|
| T01 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T02 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T03 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T04 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T05 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T06 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T07 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T08 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T09 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| **T10** | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | **Referência** |
| T11 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |
| T12 | Hz | | ☐ Sim ☐ Não | ☐ Sim ☐ Não | |

---

## 🏆 Configuração Ideal Encontrada

*(Preencher após os testes)*

```cpp
// CONFIGURAÇÃO IDEAL PARA 800Hz
byte ledBrightness = ____;
byte sampleAverage = 1;
byte ledMode = 2;
int sampleRate = 800;
int pulseWidth = ____;
int adcRange = ____;

particleSensor.setPulseAmplitudeRed(____);
particleSensor.setPulseAmplitudeIR(____);
```

---

## 💡 Lições Aprendidas

1. **Versionamento é essencial** - Sempre commitar antes de experimentos
2. **Documentar configurações** - Anotar parâmetros de sessões bem-sucedidas
3. **Teste sistemático** - Quando perdido, criar matriz de testes controlados
4. **Backups automáticos** - Considerar backup automático de configs funcionais

---

## 🔗 Arquivos Relacionados

- [Firmware v14 Test Matrix](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/firmware/PulseAnalytics_v14_test_matrix/PulseAnalytics_v14_test_matrix.ino)
- [Documentação do Sensor](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/docs/SENSOR_CONFIG_EXPLAINED.md)

---

*Documento criado em 2026-01-17*
