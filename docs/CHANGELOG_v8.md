# Changelog v8.0 - MAX30102 @ 400Hz

**Data:** 14/01/2026

## ⚠️ Descoberta Importante

O sensor do projeto é **MAX30102** (não MAX30105). 
- **MAX30102:** Red + IR apenas
- **MAX30105:** Red + IR + Green

A v7.0 (Green LED) era incompatível e foi marcada como obsoleta.

---

## Novo Firmware: v8.0

### Arquivo
`firmware/PulseAnalytics_v8_cloud/PulseAnalytics_v8_cloud.ino`

### Configuração
| Parâmetro | Valor |
|-----------|-------|
| ledMode | 2 (Red + IR) |
| sampleRate | 400 Hz |
| pulseWidth | 69 (15-bit) |
| sampleAverage | 1 |
| I2C Clock | 400kHz |
| Duração | 60s (inicial) |

### Características
- ✅ Buffer local (24000 amostras)
- ✅ Upload único no final
- ✅ Sem cálculos no device
- ✅ Comandos Serial completos

---

## Bug Corrigido

**Problema:** Leitura incorreta do FIFO do sensor causava coleta de apenas ~1500 amostras.

**Solução:** Alterado para usar `getFIFOIR()` / `getFIFORed()` com loop `while(available())`.

---

## Comandos Serial

```
start / s    - Iniciar coleta
c            - Resetar sessões
l            - Ver log
USER:nome    - Definir usuário
TAG:tag      - Definir tag
AGE:idade    - Definir idade
SEX:m/f      - Definir sexo
status       - Ver status
help         - Ver comandos
```

---

## Próximos Passos

1. Validar coleta de 60 segundos
2. Aumentar para 300 segundos (5 min)
3. Testar upload completo para Supabase
