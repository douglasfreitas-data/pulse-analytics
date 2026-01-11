# 🎓 Insights para TCC - Pulse Analytics

Este documento reúne os desafios técnicos enfrentados, os erros diagnosticados e propostas inovadoras para implementação futura, servindo como base para a fundamentação técnica do seu Trabalho de Conclusão de Curso.

---

## 🛠️ 1. Desafios de Engenharia & Soluções (O que deu errado e como resolvemos)

### A. Estouro de Memória em Microcontroladores (Heap Fragmentation)
*   **O Problema**: A tentativa de coletar 60 segundos de dados brutos a 200Hz gerou 3 vetores de 12.000 pontos (36.000 inteiros). Ao tentar serializar isso em uma única string JSON para envio, o tamanho excedia 200KB. O ESP32 (sem PSRAM externa) possui apenas ~320KB de RAM total, mas muito menos disponível contiguamente, causando *crashes* silenciosos ou falha na alocação da String.
*   **A Solução Técnica**: Implementação de **Transmission Control Protocol (TCP) Stream com Chunked Encoding**.
    *   Em vez de alocar o pacote inteiro na RAM, reescrevemos o cliente HTTP (`NetworkClientSecure`) para enviar os dados em "fatias" (chunks) de 50 amostras.
    *   Isso transformou um problema de *Capacidade de Memória* (O(n)) em um problema de *Tempo de Transmissão* (O(1) em memória), viabilizando Big Data em hardware modesto.

### B. Limitações de Algoritmos Embarcados (Zero-Crossing)
*   **O Problema**: O firmware v6 reportava BPMs irreais (ex: 179 BPM em repouso).
*   **Causa Raiz**: O algoritmo simples de contagem de picos no ESP32 confundia a **Onda Dicrótica** (segundo pico natural da pressão arterial durante a diástole) e ruídos de eletromagnéticos com batimentos cardíacos reais.
*   **Conclusão para TCC**: Microcontroladores devem atuar como *Data Loggers* (coleta fidedigna), delegando o processamento complexo (FFT, Wavelet, Pan-Tompkins) para a nuvem ou processadores mais potentes (Edge Gateway), garantindo "Clean Data" para Data Science.

### C. Gestão de Energia em Sensores Ópticos
*   **O Problema**: Ao ativar o terceiro canal (LED Verde) para leitura completa, notou-se uma queda na intensidade lida dos outros canais.
*   **Análise**: O aumento da demanda de corrente instantânea no barramento I2C/3.3V afeta a emissão dos LEDs se a regulação não for perfeita.
*   **Ajuste**: Calibração adaptativa do `FINGER_THRESHOLD` via software foi necessária.

---

## 🚀 2. Propostas de Implementação Futura (Diferenciais para o TCC)

Estas são ideias que você pode citar como "Trabalhos Futuros" ou implementar para ganhar nota máxima.

### A. Cancelamento Ativo de Artefatos de Movimento (ANC Óptico)
*   **Hipótese**: A luz **Verde** (537nm) tem menor penetração na pele que o Vermelho/Infravermelho, refletindo mais a superfície.
*   **Implementação**: Utilizar o sinal do canal Verde como referência de "Ruído de Movimento". Subtrair o sinal Verde normalizado do sinal Infravermelho (Invertido/Faseado) para isolar o componente pulsátil arterial puro, permitindo medições durante exercícios físicos (corrida).

### B. Protocolos de Alta Eficiência (Binário vs Texto)
*   **Problema**: JSON é ineficiente (texto ASCII). Enviar "16384" gasta 5 bytes, enquanto o valor binário `0x4000` gasta apenas 2 bytes.
*   **Proposta**: Substituir JSON por **MessagePack** ou **Protocol Buffers (ProtoBuf)**.
*   **Impacto Esperado**: Redução de ~60% no uso de dados (crucial para aplicações IoT via 4G/NB-IoT) e redução do tempo de upload pela metade.

### C. Edge AI (TinyML)
*   **Proposta**: Treinar uma rede neural pequena (TensorFlow Lite for Microcontrollers) para rodar dentro do ESP32.
*   **Função**: O modelo classificaria a qualidade do sinal em tempo real (ex: "Sinal Limpo" vs "Muitos Artefatos").
*   **Benefício**: O dispositivo só gastaria bateria enviando dados para a nuvem se a qualidade fosse classificada como "Utilizável" para diagnósticos médicos.

### D. Fusão de Sensores (Sensor Fusion)
*   **Ideia**: Adicionar um Acelerômetro (MPU6050) ao sistema.
*   **Aplicação**: Cruzar os dados de aceleração (eixo Z) com os picos espúrios do PPG. Se houver um pico no PPG no exato momento de um pico de aceleração, o algoritmo descarta aquele batimento como "passada/movimento" e não arritmia.

---

## 📚 Referências Teóricas Sugeridas
1.  **Pan, J., & Tompkins, W. J. (1985).** A Real-Time QRS Detection Algorithm.
2.  **Allen, J. (2007).** Photoplethysmography and its application in clinical physiological measurement. (Base para entender a física da luz na pele).
