/**
 * ============================================
 * SENSOR RESET - FAXINA TOTAL
 * ============================================
 * 
 * OBJETIVO: Limpar estado do MAX30102 após travamento
 * 
 * PROCEDIMENTO:
 * 1. Desconecte o USB por 1 minuto (limpeza física)
 * 2. Reconecte e carregue este sketch
 * 3. Aguarde a mensagem "SENSOR LIMPO!"
 * 4. Carregue o firmware de teste (v14)
 */

#include <Wire.h>
#include <MAX30105.h>

MAX30105 particleSensor;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Tempo para abrir Serial Monitor
  
  Serial.println("\n============================================");
  Serial.println("   FAXINA TOTAL - MAX30102 RESET");
  Serial.println("============================================\n");
  
  // Iniciar I2C
  Wire.begin();
  Wire.setClock(400000);
  
  Serial.println("1. Iniciando comunicação I2C...");
  delay(500);
  
  // Verificar se sensor está presente
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("   ERRO: Sensor não encontrado!");
    Serial.println("   Verifique conexões e tente novamente.");
    while(1);
  }
  Serial.println("   OK - Sensor detectado!");
  
  // ============================================
  // FASE 1: Soft Reset (limpa registradores)
  // ============================================
  Serial.println("\n2. Executando softReset()...");
  particleSensor.softReset();
  delay(1000);  // Tempo para o sensor se recuperar
  Serial.println("   OK - Registradores limpos!");
  
  // ============================================
  // FASE 2: Desligar LEDs
  // ============================================
  Serial.println("\n3. Desligando todos os LEDs...");
  particleSensor.setPulseAmplitudeRed(0);
  particleSensor.setPulseAmplitudeIR(0);
  particleSensor.setPulseAmplitudeGreen(0);
  delay(500);
  Serial.println("   OK - LEDs desligados!");
  
  // ============================================
  // FASE 3: Limpar FIFO
  // ============================================
  Serial.println("\n4. Limpando buffer FIFO...");
  particleSensor.clearFIFO();
  delay(500);
  Serial.println("   OK - FIFO limpo!");
  
  // ============================================
  // FASE 4: Configuração mínima de teste
  // ============================================
  Serial.println("\n5. Aplicando configuração mínima...");
  
  // Configuração básica só para verificar que está funcionando
  byte ledBrightness = 0x1F;  // Bem baixo para teste
  byte sampleAverage = 1;
  byte ledMode = 2;           // Red + IR
  int sampleRate = 100;       // Baixa taxa para teste
  int pulseWidth = 411;       // Máxima resolução
  int adcRange = 4096;
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  
  delay(500);
  Serial.println("   OK - Configuração aplicada!");
  
  // ============================================
  // FASE 5: Teste de leitura
  // ============================================
  Serial.println("\n6. Testando leitura do sensor...");
  delay(100);
  
  particleSensor.check();
  
  if (particleSensor.available()) {
    uint32_t ir = particleSensor.getFIFOIR();
    uint32_t red = particleSensor.getFIFORed();
    particleSensor.nextSample();
    
    Serial.print("   IR: "); Serial.println(ir);
    Serial.print("   Red: "); Serial.println(red);
    
    if (ir > 1000 && red > 1000) {
      Serial.println("   OK - Sensor respondendo!");
    } else {
      Serial.println("   AVISO: Valores baixos (normal sem dedo)");
    }
  } else {
    Serial.println("   Aguardando dados...");
  }
  
  // ============================================
  // RESULTADO FINAL
  // ============================================
  Serial.println("\n============================================");
  Serial.println("   FAXINA CONCLUÍDA COM SUCESSO!");
  Serial.println("============================================");
  Serial.println("\nPróximos passos:");
  Serial.println("1. Desconecte o USB por 10 segundos");
  Serial.println("2. Reconecte");
  Serial.println("3. Carregue o firmware v14_test_matrix");
  Serial.println("4. Execute: t10 (configuração Session 18)");
  Serial.println("\n============================================\n");
}

void loop() {
  // Piscar LED para indicar que terminou
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    ledState = !ledState;
    
    // Alternar LEDs do sensor como indicador visual
    if (ledState) {
      particleSensor.setPulseAmplitudeIR(0x10);
    } else {
      particleSensor.setPulseAmplitudeIR(0x00);
    }
  }
  
  // Mostrar leituras contínuas para verificar funcionamento
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    
    particleSensor.check();
    if (particleSensor.available()) {
      uint32_t ir = particleSensor.getFIFOIR();
      uint32_t red = particleSensor.getFIFORed();
      particleSensor.nextSample();
      
      Serial.print("Leitura: IR=");
      Serial.print(ir);
      Serial.print(" Red=");
      Serial.print(red);
      
      if (ir > 50000) {
        Serial.println(" [DEDO DETECTADO]");
      } else {
        Serial.println(" [sem dedo]");
      }
    }
  }
}
