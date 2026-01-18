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

## ✅ SOLUÇÃO ENCONTRADA: softReset()

O problema foi resolvido adicionando `particleSensor.softReset()` no setup do firmware!

### O que aconteceu
O sensor MAX30102 ficou com registradores internos "travados" após um travamento forçado (desconexão USB durante execução). O `softReset()` limpa esses registradores e restaura o sensor ao estado de fábrica.

### Código da solução (v11)
```cpp
// Após particleSensor.begin()
Serial.println("Executando softReset()...");
particleSensor.softReset();
delay(500);
Serial.println("Sensor resetado!");
```

### Resultado
- ✅ Sinal PPG restaurado com amplitude completa (0.0 - 1.0)
- ✅ Picos bem definidos e íngremes
- ✅ Dicrótico notch visível
- ✅ Taxa real próxima de 800Hz

---

## 🧪 Fase 2: Matriz de Refinamento

Agora que o sinal básico funciona, criamos uma matriz para encontrar a configuração **ótima**.

### Firmware v14 - Refinement Matrix

Parâmetros fixos (funcionaram bem):
- `pulseWidth = 215` (máximo para 800Hz)
- `adcRange = 16384` (máximo range)
- `redAmplitude = 0x7F`

Parâmetros variáveis:
- `irAmplitude`: 0x60 a 0x80
- `ledBrightness`: 0x60 a 0xA0

### Configurações de Refinamento

| # | Nome | LED | IR | Grupo |
|---|------|-----|-----|-------|
| 1 | R01_IR60_LED7F | 0x7F | 0x60 | IR baixo |
| 2 | R02_IR68_LED7F | 0x7F | 0x68 | IR intermediário- |
| **3** | **R03_IR70_LED7F** | **0x7F** | **0x70** | **REFERÊNCIA (v11)** |
| 4 | R04_IR78_LED7F | 0x7F | 0x78 | IR intermediário+ |
| 5 | R05_IR80_LED7F | 0x7F | 0x80 | IR alto |
| 6 | R06_IR70_LED60 | 0x60 | 0x70 | LED baixo |
| 7 | R07_IR70_LED70 | 0x70 | 0x70 | LED médio |
| 8 | R08_IR70_LED90 | 0x90 | 0x70 | LED alto |
| 9 | R09_IR70_LEDA0 | 0xA0 | 0x70 | LED muito alto |
| 10 | R10_IR68_LED90 | 0x90 | 0x68 | LED alto + IR baixo |
| 11 | R11_IR78_LED60 | 0x60 | 0x78 | LED baixo + IR alto |
| 12 | R12_IR75_LED80 | 0x80 | 0x75 | Equilíbrio otimizado |

> [!IMPORTANT]
> O **Teste R03** é a configuração atual do v11 e serve como baseline.

---

## 📊 Como Executar os Testes

### 1. Flash do Firmware v14

### 2. Comandos Disponíveis

| Comando | Descrição |
|---------|-----------|
| `t1` a `t12` | Executar teste específico |
| `auto` | Executar todos os 12 testes sequencialmente |
| `configs` | Listar todas as configurações |
| `retry` | Reenviar upload que falhou |
| `help` | Mostrar ajuda |

### 3. Procedimento de Teste

1. Conectar ESP32 via Serial (115200 baud)
2. Digitar `t3` para testar a referência primeiro
3. Se OK, digitar `auto` para todos os testes
4. Manter o dedo no sensor durante cada coleta (10 segundos)
5. Verificar resultados no Supabase

---

## 📈 Análise dos Resultados

### O que procurar

```
Sinal BOM:                    Sinal RUIM:
    /\    /\    /\               ___________
   /  \  /  \  /  \             /           
  /    \/    \/    \           /            
                               
- Picos claros                - Sem picos
- Periodicidade visível       - Sinal plano
- Amplitude 0.0-1.0           - Saturação (teto)
```

### Critérios de Sucesso

- [x] Picos PPG visíveis e bem definidos ✅ (resolvido com softReset)
- [ ] Taxa efetiva = 800Hz
- [ ] Sem saturação
- [ ] Maior amplitude possível sem saturar

---

## 📝 Registro de Resultados (Refinamento)

| Teste | Taxa Real | Amplitude | Picos | Saturação | Nota |
|-------|-----------|-----------|-------|-----------|------|
| R01 | Hz | | ☐ | ☐ | |
| R02 | Hz | | ☐ | ☐ | |
| **R03** | **Hz** | | ☐ | ☐ | **Referência** |
| R04 | Hz | | ☐ | ☐ | |
| R05 | Hz | | ☐ | ☐ | |
| R06 | Hz | | ☐ | ☐ | |
| R07 | Hz | | ☐ | ☐ | |
| R08 | Hz | | ☐ | ☐ | |
| R09 | Hz | | ☐ | ☐ | |
| R10 | Hz | | ☐ | ☐ | |
| R11 | Hz | | ☐ | ☐ | |
| R12 | Hz | | ☐ | ☐ | |

---

## 🏆 Configuração Atual (v11 - Funcionando)

```cpp
// CONFIGURAÇÃO FUNCIONAL PARA 800Hz
byte ledBrightness = 0x7F;
byte sampleAverage = 1;
byte ledMode = 2;
int sampleRate = 800;
int pulseWidth = 215;
int adcRange = 16384;

particleSensor.setPulseAmplitudeRed(0x7F);
particleSensor.setPulseAmplitudeIR(0x70);

// IMPORTANTE: softReset() antes de configurar!
particleSensor.softReset();
delay(500);
```

---

## 💡 Lições Aprendidas

1. **softReset() é essencial** - Sempre limpar o sensor após travamentos
2. **Versionamento é essencial** - Sempre commitar antes de experimentos
3. **Documentar configurações** - Anotar parâmetros de sessões bem-sucedidas
4. **Teste sistemático** - Quando perdido, criar matriz de testes controlados
5. **Backups automáticos** - Considerar backup automático de configs funcionais

---

## 🔗 Arquivos Relacionados

- [Firmware v11 (funcionando)](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/firmware/PulseAnalytics_v11_800Hz/PulseAnalytics_v11_800Hz.ino)
- [Firmware v14 Test Matrix](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/firmware/PulseAnalytics_v14_test_matrix/PulseAnalytics_v14_test_matrix.ino)
- [Documentação do Sensor](file:///home/douglas/Documentos/Projects/PPG/pulse-analytics/docs/SENSOR_CONFIG_EXPLAINED.md)

---

*Documento atualizado em 2026-01-17*
