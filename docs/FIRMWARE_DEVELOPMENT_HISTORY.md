# Histórico de Desenvolvimento do Firmware PPG

> Documentação técnica para futuro artigo científico

## Timeline de Versões

### Fase 1: Primeiras Versões (v1-v14)
- Desenvolvimento inicial do sistema de coleta
- Testes com diferentes taxas de amostragem
- Experimentos com configurações do MAX30102

### Fase 2: Otimização de Qualidade (v15.x)

#### v15.0 Optimal
- **Objetivo**: Máxima qualidade de sinal
- **Taxa**: 800Hz configurado → ~757Hz real
- **Configuração**: pulseWidth=215µs, sampleAverage=1
- **Resultado**: Excelente qualidade, mas buffer limitado

#### v15.1 Centered
- **Base**: v15.0
- **Melhorias**: 
  - Display OLED centralizado (para telas cortadas)
  - Botão BOOT como retry
- **Armazenamento**: RAM interna (40.000 amostras)
- **Problema**: Buffer cheio antes de upload → perda de dados

#### v15.2 Reliable
- **Foco**: Confiabilidade de upload
- **Melhorias**: Gestão de conexão WiFi

### Fase 3: Expansão de Capacidade (v17.x)

#### v17-800Hz-PSRAM
- **Objetivo**: Coletas mais longas usando PSRAM
- **Armazenamento**: PSRAM (50.000 amostras)
- **Melhorias**:
  - Buffers dinâmicos na PSRAM
  - Backoff exponencial em retries
  - Cleanup de conexões
- **Problema Identificado**: Instabilidade nos dados
  - Causa: Latência da PSRAM (~40-60ns vs ~10ns RAM)
  - Efeito: Overflow ocasional do FIFO do sensor

### Fase 4: Arquitetura Híbrida (v18)

#### v18 Hybrid (2026-01-31)
- **Solução**: Dual-core com buffer intermediário
- **Arquitetura**:
  - Core 0: Coleta rápida → Ring buffer (RAM, 8000 amostras)
  - Core 1: Transferência paralela → PSRAM (50.000 amostras)
- **Resultado**: Combina velocidade da RAM com capacidade da PSRAM
- **Status**: ✅ Testado e funcional

---

## Descobertas de Hardware

### Posicionamento do Sensor
| Versão do Clipe | Dedo Ideal | Observação |
|-----------------|------------|------------|
| Original | Mindinho | Sensor alinhado com superfície |
| Modificado (cavidade) | Indicador | Maior volume preenche cavidade |

### Configuração do MAX30102
```cpp
ledBrightness = 0x90;
sampleAverage = 1;
ledMode = 2;        // Red + IR
sampleRate = 800;   // → ~757Hz real
pulseWidth = 215;   // 18-bit ADC
adcRange = 16384;
```

---

## Lições Aprendidas

1. **RAM vs PSRAM**: Latência importa para coleta em tempo real
2. **Dual-Core**: ESP32-S3 permite separar tarefas críticas
3. **Ring Buffer**: Padrão produtor-consumidor resolve o trade-off
4. **Hardware**: Anatomia do dedo afeta qualidade do sinal

---

## Próximas Etapas
- [ ] Testes com coletas mais longas (5+ minutos)
- [ ] Fine-tuning do modelo com dados anotados
- [ ] Validação cruzada com oxímetros comerciais
