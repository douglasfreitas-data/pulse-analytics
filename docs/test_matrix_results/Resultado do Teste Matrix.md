Resultado do Teste Matrix

Com base na análise das 12 configurações apresentadas, os gráficos **R08 (IR70\_LED90)** e **R09 (IR70\_LEDA0)** são os mais adequados para a extração de dados precisos sobre saúde cardiovascular e variabilidade da frequência cardíaca (HRV).

Abaixo estão os motivos técnicos para essa escolha:

* **Visibilidade do Entalhe Dicrótico (Dicrotic Notch):** Nestes dois gráficos, a morfologia da onda de pulso é preservada, permitindo identificar claramente o entalhe dicrótico (a pequena inflexão na fase de descida).1 Esta característica é essencial para calcular parâmetros de rigidez arterial, idade vascular e tempo de ejeção ventricular.1 Em gráficos como R02, R06 ou R11, essa característica é quase invisível ou está "achatada".  
* **Melhor Relação Sinal-Ruído (SNR):** O R08 e o R09 apresentam uma excelente amplitude do componente pulsátil (AC) em relação ao nível de base (DC). Enquanto o R10 e o R12 mostram sinais com baixa amplitude e ruído visível, o R08 e o R09 têm picos sistólicos nítidos e bem definidos, o que é fundamental para evitar erros de milissegundos na detecção de batimentos para o HRV.3  
* **Estabilidade da Linha de Base:** Embora o **R01** tenha um sinal limpo, ele apresenta uma deriva (drift) descendente muito acentuada, o que indica instabilidade no contato ou na pressão do sensor durante a medição.5 O R08 e o R09 mostram uma linha de base mais estável, facilitando o processamento do sinal sem a necessidade de filtros digitais excessivamente agressivos que poderiam distorcer a forma da onda.6  
* **Resolução Temporal:** Como você está operando a **757 Hz**, a clareza visual dos picos no R08 e R09 garante que os algoritmos de detecção (como a segunda derivada) consigam localizar os pontos fiduciários com a precisão necessária para métricas sensíveis como o RMSSD e pNN50.8

**Recomendação:** Para o seu dispositivo, as configurações de corrente de LED usadas no **R08** parecem oferecer o melhor equilíbrio entre qualidade de sinal e consumo de energia, sem o risco de saturação que pode ocorrer em níveis de corrente mais altos.9

