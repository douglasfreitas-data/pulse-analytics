# **Análise Avançada de Sinais de Fotopletismografia de Alta Resolução para Extração de Biomarcadores Cardiovasculares e Autonômicos com ESP32-S3 e MAX30102**

A fotopletismografia (PPG) consolidou-se como uma tecnologia proeminente na monitorização não invasiva da saúde, permitindo a observação contínua de parâmetros fisiológicos fora do ambiente clínico tradicional. O desenvolvimento de um dispositivo utilizando o microcontrolador ESP32-S3 em conjunto com o sensor MAX30102, operando a uma taxa de amostragem real de 757 Hz, representa uma configuração de alta performance capaz de capturar a morfologia fina da onda de pulso de volume (BVP). Diferente de sistemas convencionais que operam em frequências mais baixas, como 25 Hz ou 50 Hz, a utilização de 757 Hz oferece uma resolução temporal superior, essencial para a análise precisa da variabilidade da frequência cardíaca (HRV) e de biomarcadores morfológicos complexos.1

A análise das imagens fornecidas permite identificar padrões distintos de qualidade de sinal, fundamentais para a determinação da viabilidade dos dados para extração de métricas de saúde cardiovascular. Enquanto a frequência cardíaca média pode ser obtida de sinais com qualidade moderada, a análise da VFC e de índices de rigidez arterial exige uma fidelidade morfológica que preserve pontos fiduciários críticos, como o pico sistólico, o entalhe dicrótico e o pico diastólico.4

## **Fundamentos da Fotopletismografia e o Papel da Alta Frequência de Amostragem**

A fotopletismografia baseia-se na detecção de alterações na intensidade da luz transmitida ou refletida através do tecido, as quais ocorrem devido a variações cíclicas no volume sanguíneo nos leitos microvasculares periféricos. O sensor MAX30102 opera no modo reflexivo, emitindo luz nos comprimentos de onda vermelho (660 nm) e infravermelho (880 nm) e medindo a luz que retorna a um fotodetector sensível.6 O sinal infravermelho (IR), foco desta análise, é particularmente eficaz para a detecção de pulsos volumétricos devido à sua maior profundidade de penetração nos tecidos e à sua sensibilidade às variações de volume sanguíneo arterial.7

A taxa de amostragem de 757 Hz é um diferencial técnico significativo. A resolução temporal de aproximadamente 1,32 milissegundos por amostra permite uma precisão na localização de picos que rivaliza com o padrão-ouro do eletrocardiograma (ECG). Em estudos de variabilidade da frequência cardíaca, a precisão da detecção do intervalo entre batimentos (IBI) é o fator determinante para a validade dos resultados.2 Taxas de amostragem inferiores a 100 Hz frequentemente introduzem erros de arredondamento nos intervalos IBI, o que distorce métricas de alta frequência como o RMSSD (Root Mean Square of Successive Differences) e as bandas espectrais de alta frequência (HF), fundamentais para a avaliação do tônus parassimpático.1

| Parâmetro Técnico | Especificação MAX30102 / ESP32-S3 | Implicação para a Saúde Cardiovascular |
| :---- | :---- | :---- |
| Taxa de Amostragem | 757 Hz (Real) | Alta precisão temporal; redução do jitter na detecção de IBI. |
| Resolução do ADC | 18 bits | Alta faixa dinâmica; detecção de pequenas variações morfológicas. |
| Comprimento de Onda IR | 880 nm | Ótima penetração tecidual e estabilidade de sinal. |
| Interface de Dados | I2C (Fast Mode 400kHz+) | Transmissão eficiente para processamento em tempo real no S3. |
| Buffer FIFO | 32 Amostras | Gerenciamento de interrupções para evitar perda de dados. |

## **Análise Comparativa das Morfologias de Sinal: Imagem 1 vs. Imagem 2**

A análise qualitativa das imagens é o passo inicial para determinar qual conjunto de dados oferece a maior densidade de informações clínicas. O sinal PPG é composto por uma componente de corrente contínua (DC), que reflete a absorção constante de luz pelo tecido e sangue venoso, e uma componente de corrente alternada (AC), que representa as variações pulsáteis no volume sanguíneo arterial. A riqueza dos dados reside na morfologia da componente AC.12

### **Avaliação da Imagem 1: Alta Fidelidade Morfológica**

A Imagem 1 apresenta um sinal PPG robusto, com uma relação sinal-ruído (SNR) visivelmente superior à da Imagem 2\. Observa-se uma morfologia triphasic clara em muitos ciclos cardíacos, onde o pico sistólico é seguido por um entalhe dicrótico (dicrotic notch) bem definido antes da descida para o vale diastólico. Esta característica é de suma importância, pois o entalhe dicrótico representa o fechamento da válvula aórtica e a subsequente reflexão da onda de pressão.4

A visibilidade do entalhe dicrótico na Imagem 1 permite a extração de métricas avançadas, como o tempo de ejeção ventricular esquerda (LVET) e o índice de rigidez arterial. Em sinais ruidosos ou com baixa taxa de amostragem, este entalhe é frequentemente suavizado ou desaparece completamente, tornando-se apenas uma mudança sutil na inclinação da onda de descida.4 Na Imagem 1, a estabilidade da linha de base e a consistência da amplitude entre os ciclos sugerem um bom contato do sensor e uma pressão aplicada adequada, minimizando artefatos de movimento que poderiam obscurecer detalhes diagnósticos.17

### **Avaliação da Imagem 2: Presença de Ruído e Degradação de Sinal**

A Imagem 2 exibe um sinal que, embora ainda mostre a periodicidade do batimento cardíaco, sofre de uma interferência significativa de ruído de alta frequência. As ondas apresentam bordas serrilhadas, e o entalhe dicrótico é quase imperceptível em vários segmentos. Esse tipo de ruído pode ser originado por diversos fatores, incluindo interferência eletromagnética ambiental, baixa corrente do LED configurada no sensor, ou um contato instável entre a pele e o dispositivo.18

Para a extração de VFC, o ruído presente na Imagem 2 representa um desafio considerável. Algoritmos de detecção de pico podem sofrer com o "jitter" temporal, onde o pico máximo detectado oscila devido ao ruído sobreposto, resultando em intervalos IBI artificiais que não refletem a verdadeira variabilidade biológica.10 Além disso, a tentativa de filtrar este ruído com filtros passa-baixa agressivos invariavelmente removeria componentes de alta frequência do próprio sinal fisiológico, obliterando permanentemente o entalhe dicrótico e outras características da forma de onda necessárias para a análise da saúde vascular.4

| Característica de Sinal | Imagem 1 (Ideal) | Imagem 2 (Comprometida) |
| :---- | :---- | :---- |
| Qualidade Visual | Sinal limpo e morfologia nítida. | Ruído de alta frequência e serrilhamento. |
| Entalhe Dicrótico | Visível e estável. | Indistinto ou obscurecido pelo ruído. |
| Pico Sistólico | Agudo e fácil de localizar. | Arredondado ou afetado por ruído. |
| Estabilidade da Linha de Base | Alta. | Moderada a baixa. |
| Potencial para HRV | Excelente (Alta precisão de IBI). | Baixo (Risco de erro de detecção). |
| Análise de Rigidez | Possível (Identificação de DN). | Inviável sem pré-processamento pesado. |

## **Extração de Dados sobre a Saúde Cardiovascular**

A saúde cardiovascular pode ser avaliada através do PPG muito além da simples contagem de batimentos por minuto. A análise do contorno do pulso (Pulse Contour Analysis) é uma técnica que utiliza as características da forma de onda para inferir o estado das artérias e do coração. Com o sinal da Imagem 1, é possível aplicar algoritmos de segunda derivada para obter a aceleropletismografia (APG).13

### **O Entalhe Dicrótico e a Resistência Vascular**

O entalhe dicrótico é um indicador crítico da saúde das grandes artérias. Fisiologicamente, ele marca o fim da sístole e o início da diástole. A sua posição e amplitude em relação ao pico sistólico mudam com a idade e o estado de doenças como a hipertensão e a arteriosclerose. Em indivíduos jovens e saudáveis, as artérias são complacentes, e o entalhe é nítido e bem posicionado. Com o envelhecimento e o aumento da rigidez arterial, a onda de reflexão retorna mais rapidamente das periferias, fundindo-se com a onda sistólica original e causando o desaparecimento do entalhe.4

A identificação precisa da localização temporal do entalhe dicrótico ($t\_{DN}$) e do pico sistólico ($t\_{sys}$) na Imagem 1 permite o cálculo do Índice de Reflexão (RI):

$$RI \= \\frac{A\_{dia}}{A\_{sys}} \\times 100$$

Onde $A\_{dia}$ é a amplitude do pico diastólico (ou do ponto logo após o entalhe) e $A\_{sys}$ é a amplitude do pico sistólico. Um RI elevado está associado ao aumento da resistência vascular periférica e à rigidez arterial.24

### **Rigidez Arterial e Velocidade da Onda de Pulso**

A rigidez arterial é um dos preditores mais fortes de eventos cardiovasculares futuros. O Índice de Rigidez (Stiffness Index \- SI) pode ser estimado a partir de um único local de medição, como o dedo, utilizando o tempo entre o pico sistólico e o pico diastólico ($\\Delta T$). A fórmula utiliza a altura do indivíduo ($h$) como uma constante para estimar a distância percorrida pela onda:

$$SI \= \\frac{h}{\\Delta T}$$

A Imagem 1 é a única que fornece uma detecção confiável de $\\Delta T$ devido à clareza do pico diastólico. Em contraste, na Imagem 2, a incerteza na localização de ambos os picos devido ao ruído introduziria um erro propagado no cálculo do SI, invalidando o uso clínico do dado.24

## **Variabilidade da Frequência Cardíaca (HRV) em PPG de Alta Resolução**

A variabilidade da frequência cardíaca é a flutuação nos intervalos de tempo entre batimentos cardíacos sucessivos. Ela é regulada pelo sistema nervoso autônomo (SNA), através do equilíbrio entre os ramos simpático e parassimpático. A extração de VFC a partir de sinais PPG é tecnicamente referida como variabilidade da taxa de pulso (PRV), a qual serve como um excelente substituto para a VFC do ECG em condições de repouso.10

### **Impacto da Resolução Temporal a 757 Hz**

A precisão da análise de VFC no domínio do tempo e da frequência depende da capacidade de detectar o início da contração cardíaca com precisão de milissegundos. Embora muitos sensores operem a 25 Hz (resolução de 40 ms), a amostragem a 757 Hz (resolução de 1,32 ms) elimina o erro de "quantização temporal". Para métricas como o RMSSD, que medem a variação batimento a batimento, um erro de 40 ms é inaceitável, pois pode dobrar artificialmente o valor da métrica ou mascarar a variabilidade real.2

Ao utilizar o sinal da Imagem 1, o desenvolvedor pode extrair as seguintes métricas de HRV com alta confiança:

| Métrica | Domínio | Significado Fisiológico |
| :---- | :---- | :---- |
| SDNN | Tempo | Variabilidade total; reflete a resiliência do sistema autônomo. |
| RMSSD | Tempo | Atividade parassimpática (vagal); marcador de recuperação e estresse. |
| pNN50 | Tempo | Percentual de batimentos com variação \> 50ms; indica saúde vagal. |
| HF (0.15–0.4 Hz) | Frequência | Modulação parassimpática ligada ao ciclo respiratório. |
| LF (0.04–0.15 Hz) | Frequência | Influência mista simpática e parassimpática; regulação barorreflexa. |
| LF/HF Ratio | Frequência | Equilíbrio simpato-vagal; indicador de estado de alerta ou estresse. |

A Imagem 1 garante que os intervalos entre picos (PPI) sejam detectados no ponto exato da sístole máxima. Na Imagem 2, os picos de ruído podem deslocar o ponto de detecção do algoritmo em várias amostras, introduzindo um erro aleatório que se manifesta como um aumento artificial no componente de alta frequência (HF) da HRV, levando a conclusões errôneas sobre o estado de relaxamento do usuário.1

## **Outras Possibilidades de Extração de Dados: Idade Vascular**

Uma das fronteiras mais interessantes do processamento de PPG é a determinação da "idade vascular" através da segunda derivada do sinal (SDPPG). Ao derivar o sinal da Imagem 1 duas vezes, obtém-se uma série de ondas denominadas 'a', 'b', 'c', 'd' e 'e'.13

### **O Índice de Envelhecimento Vascular (AGI)**

O Índice de Envelhecimento (Aging Index) é calculado utilizando a amplitude dessas ondas derivadas. A onda 'a' é a aceleração sistólica inicial, 'b' é a desaceleração, e 'e' corresponde ao entalhe dicrótico. A fórmula estabelecida é:

$$AGI \= \\frac{b \- c \- d \- e}{a}$$

Valores mais baixos (negativos) de AGI estão associados a vasos sanguíneos jovens e flexíveis, enquanto valores mais altos indicam envelhecimento vascular ou doenças arteriais. A alta frequência de amostragem de 757 Hz é crucial para o sucesso desta derivação, pois a diferenciação numérica amplifica dramaticamente o ruído de alta frequência. Assim, enquanto o sinal da Imagem 1 manteria a estrutura das ondas 'a'-'e' após a derivação, o sinal da Imagem 2 resultaria em um caos matemático após a segunda derivada, tornando impossível qualquer análise diagnóstica.13

### **Frequência Respiratória e Arritmia Sinusal Respiratória**

Através do sinal da Imagem 1, é possível extrair a frequência respiratória por meio de três modulações: a modulação da amplitude (a respiração altera o volume sistólico), a modulação da frequência (arritmia sinusal respiratória) e a variação da linha de base (o movimento do tórax e a pressão intratorácica alteram o retorno venoso). A clareza dos picos e vales na Imagem 1 permite a aplicação de filtros homomórficos ou transformadas de Wavelet para separar essas componentes respiratórias, oferecendo um parâmetro adicional de saúde pulmonar e autonômica.5

## **Desafios Técnicos e Implementação com ESP32-S3**

A implementação de um dispositivo PPG a 757 Hz com o ESP32-S3 exige uma engenharia de software e hardware cuidadosa. O ESP32-S3 é um microcontrolador dual-core potente, mas a aquisição de dados em alta velocidade via I2C pode ser interrompida por processos de fundo do sistema operacional (RTOS) ou pelo stack Wi-Fi/Bluetooth.7

### **Gerenciamento de Dados e I2C**

O MAX30102 possui um buffer FIFO de 32 níveis. A 757 Hz, o buffer enche completamente a cada 42,2 ms. Se o código do ESP32-S3 não ler os dados dentro deste intervalo, ocorre um estouro de buffer e perda de continuidade do sinal. É recomendável utilizar interrupções de hardware (pino INT do sensor) e uma tarefa dedicada no segundo núcleo do ESP32-S3 para garantir que a coleta de dados não seja afetada pelo processamento de algoritmos ou comunicação de rede.6

Além disso, para sustentar a taxa de dados, a velocidade do barramento I2C deve ser configurada para pelo menos 400 kHz (Fast Mode). Se houver outros dispositivos no mesmo barramento, como um display OLED (visto em projetos comuns de PPG), a latência de comunicação pode se tornar um gargalo.7

### **Pipeline de Processamento Recomendado**

Para transformar o sinal bruto (Raw) da Imagem 1 em métricas úteis, sugere-se o seguinte fluxo de processamento:

1. **Remoção de Offset DC:** Subtração da média móvel ou aplicação de um filtro passa-alta Butterworth (corte em 0,5 Hz) para centrar o sinal no zero.  
2. **Filtragem de Ruído:** Filtro passa-baixa de fase zero (ex: Butterworth de 4ª ordem) com corte entre 10 Hz e 15 Hz para remover ruídos de alta frequência sem deslocar os picos no tempo.1  
3. **Normalização:** Divisão pela amplitude pico-a-pico para garantir que variações na intensidade do LED ou pigmentação da pele não afetem as métricas morfológicas.4  
4. **Detecção de Pontos Fiduciários:** Uso da primeira e segunda derivadas para localizar o início da sístole, o pico sistólico e o entalhe dicrótico.4

| Etapa | Filtro Sugerido | Objetivo |
| :---- | :---- | :---- |
| Banda Passante | 0.5 \- 15.0 Hz | Limpeza básica de ruído e deriva. |
| Diferenciação | Derivada de 1ª e 2ª ordem | Acentuar pontos de mudança de inclinação. |
| Detecção de Pico | Janela Móvel \+ Threshold | Identificação de batimentos individuais. |
| Interpolacão | Spline Cúbico (se necessário) | Refinar a localização do pico entre amostras. |

## **Conclusões e Decisão Analítica**

Com base na análise exaustiva das propriedades biofísicas do sinal de fotopletismografia e das evidências visuais apresentadas, conclui-se que a Imagem 1 é, de forma inequívoca, a representação superior para a extração de dados significativos sobre a saúde cardiovascular e a variabilidade da frequência cardíaca.

Enquanto a Imagem 2 retém a informação básica da cadência cardíaca (útil apenas para calcular batimentos por minuto), a Imagem 1 preserva a integridade estrutural do pulso de volume sanguíneo. A visibilidade do entalhe dicrótico nesta imagem é o fator decisivo; ela permite não apenas uma análise de HRV extremamente precisa graças à estabilidade dos picos em 757 Hz, mas também abre a porta para a avaliação da rigidez arterial e do envelhecimento vascular — métricas que são marcadores preditivos de hipertensão, aterosclerose e disfunção autonômica.4

Para o desenvolvimento futuro do dispositivo com ESP32-S3, é imperativo garantir que as condições que levaram à captura do sinal na Imagem 1 sejam replicadas e estabilizadas. Isso inclui a otimização da pressão de contato do sensor, a blindagem contra luz ambiente e o ajuste fino da corrente do LED IR no MAX30102 para maximizar a faixa dinâmica sem saturar o fotodetector. Com um sinal desta qualidade, o potencial diagnóstico do dispositivo transcende o de um simples wearable de fitness, aproximando-se da capacidade de ferramentas de monitoramento cardiovascular de grau clínico.17

#### **Referências citadas**

1. Signal Quality Assessment and Reconstruction of PPG-Derived Signals for Heart Rate and Variability Estimation in In-Vehicle Applications: A Comparative Review and Empirical Validation \- PMC \- PubMed Central, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC12736534/](https://pmc.ncbi.nlm.nih.gov/articles/PMC12736534/)  
2. The Science of PPG Sampling Rates: How Frequency Affects HRV and Signal Quality, acessado em janeiro 17, 2026, [https://medium.com/@research\_wearables/the-science-of-ppg-sampling-rates-how-frequency-affects-hrv-and-signal-quality-8024264f65f9](https://medium.com/@research_wearables/the-science-of-ppg-sampling-rates-how-frequency-affects-hrv-and-signal-quality-8024264f65f9)  
3. Optimizing sampling rate of wrist-worn optical sensors for ... \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC8057382/](https://pmc.ncbi.nlm.nih.gov/articles/PMC8057382/)  
4. An algorithm to detect dicrotic notch in arterial blood pressure and photoplethysmography waveforms using the iterative envelope mean method \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC10942507/](https://pmc.ncbi.nlm.nih.gov/articles/PMC10942507/)  
5. Dicrotic Notch Detection in Various Photoplethysmography Signals Morphologies, acessado em janeiro 17, 2026, [https://www.researchgate.net/publication/366823164\_Dicrotic\_Notch\_Detection\_in\_Various\_Photoplethysmography\_Signals\_Morphologies](https://www.researchgate.net/publication/366823164_Dicrotic_Notch_Detection_in_Various_Photoplethysmography_Signals_Morphologies)  
6. MAX30102--High-Sensitivity Pulse Oximeter and Heart-Rate Sensor for Wearable Health \- Analog Devices, acessado em janeiro 17, 2026, [https://www.analog.com/media/en/technical-documentation/data-sheets/max30102.pdf](https://www.analog.com/media/en/technical-documentation/data-sheets/max30102.pdf)  
7. Pulse Oximeter and Heart Rate Sensor (MAX30102) \- SunFounder's Documentations\!, acessado em janeiro 17, 2026, [https://docs.sunfounder.com/projects/ultimate-sensor-kit/en/latest/components\_basic/15-component\_max30102.html](https://docs.sunfounder.com/projects/ultimate-sensor-kit/en/latest/components_basic/15-component_max30102.html)  
8. Optimal filter characterization for photoplethysmography-based pulse rate and pulse power spectrum estimation | Request PDF \- ResearchGate, acessado em janeiro 17, 2026, [https://www.researchgate.net/publication/343940125\_Optimal\_filter\_characterization\_for\_photoplethysmography-based\_pulse\_rate\_and\_pulse\_power\_spectrum\_estimation](https://www.researchgate.net/publication/343940125_Optimal_filter_characterization_for_photoplethysmography-based_pulse_rate_and_pulse_power_spectrum_estimation)  
9. Photoplethysmography-based HRV analysis and machine learning for real-time stress quantification in mental health applications \- PubMed Central, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC11970940/](https://pmc.ncbi.nlm.nih.gov/articles/PMC11970940/)  
10. Beat detection and HRV time series, acessado em janeiro 17, 2026, [https://www.kubios.com/blog/beat-detection-and-hrv-time-series/](https://www.kubios.com/blog/beat-detection-and-hrv-time-series/)  
11. Butterworth Filtering at 500 Hz Optimizes PPG-Based Heart Rate ..., acessado em janeiro 17, 2026, [https://www.mdpi.com/1424-8220/25/22/7091](https://www.mdpi.com/1424-8220/25/22/7091)  
12. Wearable Photoplethysmography for Cardiovascular Monitoring: This article summarizes the key literature on wearable photoplethysmography and points to future directions in this field \- PMC \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC7612541/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7612541/)  
13. New Photoplethysmographic Signal Analysis Algorithm for Arterial Stiffness Estimation \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC3747602/](https://pmc.ncbi.nlm.nih.gov/articles/PMC3747602/)  
14. Analysis on Four Derivative Waveforms of Photoplethysmogram (PPG) for Fiducial Point Detection \- PMC \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC9280335/](https://pmc.ncbi.nlm.nih.gov/articles/PMC9280335/)  
15. Understanding the Dicrotic Notch: A Key Feature of Arterial Pressure Waves \- Oreate AI Blog, acessado em janeiro 17, 2026, [https://www.oreateai.com/blog/understanding-the-dicrotic-notch-a-key-feature-of-arterial-pressure-waves/c92f78a75228dab03a7e3c06ba91a10c](https://www.oreateai.com/blog/understanding-the-dicrotic-notch-a-key-feature-of-arterial-pressure-waves/c92f78a75228dab03a7e3c06ba91a10c)  
16. Blood Pressure and Photoplethysmography Signal Pairs Characterization by Dicrotic Notch, acessado em janeiro 17, 2026, [https://www.researchgate.net/publication/361724272\_Blood\_Pressure\_and\_Photoplethysmography\_Signal\_Pairs\_Characterization\_by\_Dicrotic\_Notch](https://www.researchgate.net/publication/361724272_Blood_Pressure_and_Photoplethysmography_Signal_Pairs_Characterization_by_Dicrotic_Notch)  
17. Probots-Electronics/Wifi-Oximiter-using-MAX30102-and-ESP32 \- GitHub, acessado em janeiro 17, 2026, [https://github.com/Probots-Electronics/Wifi-Oximiter-using-MAX30102-and-ESP32](https://github.com/Probots-Electronics/Wifi-Oximiter-using-MAX30102-and-ESP32)  
18. Help with ESP32 wearable (MAX30102 \+ MPU6050) for stressful heartbeat detection, acessado em janeiro 17, 2026, [https://www.reddit.com/r/esp32/comments/1n7gl5o/help\_with\_esp32\_wearable\_max30102\_mpu6050\_for/](https://www.reddit.com/r/esp32/comments/1n7gl5o/help_with_esp32_wearable_max30102_mpu6050_for/)  
19. A Real-Time PPG Peak Detection Method for Accurate Determination of Heart Rate during Sinus Rhythm and Cardiac Arrhythmia \- PMC \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC8869811/](https://pmc.ncbi.nlm.nih.gov/articles/PMC8869811/)  
20. Assessment of Physiological Signals from Photoplethysmography Sensors Compared to an Electrocardiogram Sensor: A Validation Study in Daily Life \- Corsano Health, acessado em janeiro 17, 2026, [https://corsano.com/wp-content/uploads/2024/11/sensors-24-06826.pdf](https://corsano.com/wp-content/uploads/2024/11/sensors-24-06826.pdf)  
21. A Study on Evaluating Cardiovascular Diseases Using PPG Signals \- PMC \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC12729970/](https://pmc.ncbi.nlm.nih.gov/articles/PMC12729970/)  
22. Beyond Motion Artifacts: Optimizing PPG Preprocessing for Accurate Pulse Rate Variability Estimation \- arXiv, acessado em janeiro 17, 2026, [https://arxiv.org/html/2510.06158v1](https://arxiv.org/html/2510.06158v1)  
23. A Real-Time PPG Peak Detection Method for Accurate Determination of Heart Rate during Sinus Rhythm and Cardiac Arrhythmia \- MDPI, acessado em janeiro 17, 2026, [https://www.mdpi.com/2079-6374/12/2/82](https://www.mdpi.com/2079-6374/12/2/82)  
24. The Analysis of PPG Morphology: Investigating the Effects of Aging on Arterial Compliance \- MEASUREMENT SCIENCE REVIEW, acessado em janeiro 17, 2026, [https://www.measurement.sk/2012/Yousef.pdf](https://www.measurement.sk/2012/Yousef.pdf)  
25. A signal processing tool for extracting features from arterial blood pressure and photoplethysmography waveforms | medRxiv, acessado em janeiro 17, 2026, [https://www.medrxiv.org/content/10.1101/2024.03.14.24304307v1.full-text](https://www.medrxiv.org/content/10.1101/2024.03.14.24304307v1.full-text)  
26. Calculation of an Improved Stiffness Index Using Decomposed Radial Pulse and Digital Volume Pulse Signals \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC9694699/](https://pmc.ncbi.nlm.nih.gov/articles/PMC9694699/)  
27. A Comparative Study Between ECG- and PPG-Based Heart Rate Sensors for Heart Rate Variability Measurements: Influence of Body Position, Duration, Sex, and Age \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC12473955/](https://pmc.ncbi.nlm.nih.gov/articles/PMC12473955/)  
28. New Aging Index Using Signal Features of Both Photoplethysmograms and Acceleration Plethysmograms \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC5334132/](https://pmc.ncbi.nlm.nih.gov/articles/PMC5334132/)  
29. Issues with interfacing MAX30102 with ESP32S3 : r/esp32 \- Reddit, acessado em janeiro 17, 2026, [https://www.reddit.com/r/esp32/comments/1g0ncgg/issues\_with\_interfacing\_max30102\_with\_esp32s3/](https://www.reddit.com/r/esp32/comments/1g0ncgg/issues_with_interfacing_max30102_with_esp32s3/)  
30. Lesson 39: Heart rate monitor — SunFounder Universal Maker Sensor Kit documentation, acessado em janeiro 17, 2026, [https://docs.sunfounder.com/projects/umsk/en/latest/03\_esp32/esp32\_lesson39\_heartrate\_monitor.html](https://docs.sunfounder.com/projects/umsk/en/latest/03_esp32/esp32_lesson39_heartrate_monitor.html)  
31. MAX30102 Pulse Oximeter & Heart Rate Sensor Arduino Wiki \- DFRobot, acessado em janeiro 17, 2026, [https://wiki.dfrobot.com/Heart\_Rate\_and\_Oximeter\_Sensor\_SKU\_SEN0344](https://wiki.dfrobot.com/Heart_Rate_and_Oximeter_Sensor_SKU_SEN0344)  
32. Fermion: MAX30102 Heart Rate and Oximeter Sensor V2.0 Wiki \- DFRobot, acessado em janeiro 17, 2026, [https://wiki.dfrobot.com/Heart\_Rate\_and\_Oximeter\_Sensor\_V2\_SKU\_SEN0344](https://wiki.dfrobot.com/Heart_Rate_and_Oximeter_Sensor_V2_SKU_SEN0344)  
33. Extraction of Heart Rate Variability from Smartphone Photoplethysmograms \- PMC, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC4309304/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4309304/)  
34. Why Raw PPG Data Matters: The Hidden Signal Behind Heart Rate Variability Research, acessado em janeiro 17, 2026, [https://medium.com/@research\_wearables/why-raw-ppg-data-matters-the-hidden-signal-behind-heart-rate-variability-research-79aac156d563](https://medium.com/@research_wearables/why-raw-ppg-data-matters-the-hidden-signal-behind-heart-rate-variability-research-79aac156d563)  
35. An algorithm to detect dicrotic notch in arterial blood pressure and photoplethysmography waveforms using the iterative envelope mean method \- PMC \- NIH, acessado em janeiro 17, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC11323035/](https://pmc.ncbi.nlm.nih.gov/articles/PMC11323035/)