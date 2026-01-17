/*
 * ============================================
 * I2C STRESS TEST para MAX30105
 * ============================================
 * 
 * OBJETIVO: Validar estabilidade do barramento I2C em diferentes
 * velocidades e taxas de amostragem antes de subir para 1000Hz.
 * 
 * COMO USAR:
 * 1. Carregue este sketch no ESP32
 * 2. Abra o Serial Monitor (115200 baud)
 * 3. Observe os resultados (~45 segundos total)
 */

#include <Wire.h>
#include <MAX30105.h>

MAX30105 particleSensor;

// LED onboard para feedback visual
const int LED_PIN = 2;  // GPIO2 no ESP32 DevKit

// Contadores de teste
unsigned long totalReads = 0;
unsigned long errorCount = 0;
unsigned long lastSecond = 0;
unsigned long readsPerSecond = 0;
unsigned long maxReadsPerSecond = 0;
unsigned long minReadsPerSecond = 999999;

// Fases do teste
enum TestPhase {
  TEST_100KHZ,
  TEST_400KHZ,
  TEST_1MHZ,
  TEST_COMPLETE
};
TestPhase currentPhase = TEST_100KHZ;
unsigned long phaseStartTime = 0;
const unsigned long PHASE_DURATION = 10000; // 10 segundos por fase

bool sensorOK = false;

void setup() {
  Serial.begin(115200);
  
  // LED de feedback
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Liga LED para mostrar que iniciou
  
  delay(2000);  // Tempo para abrir Serial Monitor
  
  Serial.println();
  Serial.println("============================================");
  Serial.println("   I2C STRESS TEST - MAX30105");
  Serial.println("============================================");
  Serial.println("Testando velocidades: 100kHz, 400kHz, 1MHz");
  Serial.println("============================================");
  Serial.println();
  
  digitalWrite(LED_PIN, LOW);
  
  // Iniciar I2C padrão para primeiro teste
  Wire.begin();
  delay(100);
  
  // Testar se o sensor existe
  Serial.println("Buscando sensor MAX30105...");
  
  if (particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println(">>> SENSOR ENCONTRADO! <<<");
    sensorOK = true;
  } else {
    Serial.println(">>> ERRO: Sensor NAO encontrado! <<<");
    Serial.println("Verifique as conexoes:");
    Serial.println("  SDA -> GPIO21");
    Serial.println("  SCL -> GPIO22");
    Serial.println("  VCC -> 3.3V");
    Serial.println("  GND -> GND");
    sensorOK = false;
  }
  
  if (sensorOK) {
    startPhase(TEST_100KHZ);
  }
}

void startPhase(TestPhase phase) {
  currentPhase = phase;
  phaseStartTime = millis();
  
  // Reset contadores
  totalReads = 0;
  errorCount = 0;
  lastSecond = millis();
  readsPerSecond = 0;
  maxReadsPerSecond = 0;
  minReadsPerSecond = 999999;
  
  // Configurar velocidade I2C
  long i2cSpeed = 100000;
  String speedName = "100kHz";
  
  switch(phase) {
    case TEST_100KHZ:
      i2cSpeed = 100000;
      speedName = "100kHz (Padrao)";
      break;
    case TEST_400KHZ:
      i2cSpeed = 400000;
      speedName = "400kHz (Fast Mode) - IMPORTANTE!";
      break;
    case TEST_1MHZ:
      i2cSpeed = 1000000;
      speedName = "1MHz (Fast Mode Plus)";
      break;
    default:
      return;
  }
  
  Serial.println();
  Serial.println("--------------------------------------------");
  Serial.print("TESTANDO: ");
  Serial.println(speedName);
  Serial.println("--------------------------------------------");
  
  // Configurar I2C
  Wire.setClock(i2cSpeed);
  delay(50);
  
  // Configurar sensor - GREEN ONLY com potência MÁXIMA
  // Esta é a configuração que será usada no v7_green
  byte ledBrightness = 0xFF;  // Máximo para green
  byte sampleAverage = 1;
  byte ledMode = 3;           // Multi-LED (necessário para green)
  byte sampleRate = 400;      // 400Hz no teste, 1000Hz no final
  int pulseWidth = 69;        // Mínimo para alta frequência
  int adcRange = 4096;        // Maior sensibilidade
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // DESLIGAR Red e IR, manter apenas GREEN no máximo
  particleSensor.setPulseAmplitudeRed(0);
  particleSensor.setPulseAmplitudeIR(0);
  particleSensor.setPulseAmplitudeGreen(0xFF);  // 51mA - MÁXIMO
  
  particleSensor.clearFIFO();
  
  Serial.println("Coletando por 10 segundos...");
  Serial.println();
  Serial.println("Seg | Leituras/s | Erros | Status");
  Serial.println("----|------------|-------|--------");
}

void printPhaseResults() {
  Serial.println();
  Serial.println(">>> RESULTADO <<<");
  Serial.print("Leituras: ");
  Serial.print(totalReads);
  Serial.print(" | Erros: ");
  Serial.print(errorCount);
  Serial.print(" | Taxa erro: ");
  Serial.print((errorCount * 100.0) / max(totalReads, 1UL), 2);
  Serial.println("%");
  
  if (errorCount == 0) {
    Serial.println("STATUS: PASSOU - 0 erros!");
  } else if (errorCount < 10) {
    Serial.println("STATUS: ATENCAO - Poucos erros");
  } else {
    Serial.println("STATUS: FALHOU - Muitos erros!");
  }
}

void loop() {
  // Piscar LED para mostrar que está rodando
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    lastBlink = millis();
  }
  
  // Se sensor não foi encontrado, ficar piscando
  if (!sensorOK) {
    delay(100);
    return;
  }
  
  if (currentPhase == TEST_COMPLETE) {
    static bool printed = false;
    if (!printed) {
      Serial.println();
      Serial.println("============================================");
      Serial.println("   TESTE COMPLETO!");
      Serial.println("============================================");
      Serial.println("Se 400kHz passou, podemos usar 1000Hz!");
      Serial.println("============================================");
      printed = true;
      digitalWrite(LED_PIN, HIGH);  // LED fixo = terminou
    }
    delay(1000);
    return;
  }
  
  // Verificar se a fase terminou
  if (millis() - phaseStartTime > PHASE_DURATION) {
    printPhaseResults();
    
    switch(currentPhase) {
      case TEST_100KHZ:
        startPhase(TEST_400KHZ);
        break;
      case TEST_400KHZ:
        startPhase(TEST_1MHZ);
        break;
      case TEST_1MHZ:
        currentPhase = TEST_COMPLETE;
        break;
      default:
        break;
    }
    return;
  }
  
  // Ler do sensor - apenas GREEN
  uint32_t value = particleSensor.getGreen();
  
  if (value == 0) {
    errorCount++;
  }
  
  totalReads++;
  readsPerSecond++;
  
  // A cada segundo, mostrar estatísticas
  if (millis() - lastSecond >= 1000) {
    unsigned long second = (millis() - phaseStartTime) / 1000;
    
    if (readsPerSecond > maxReadsPerSecond) maxReadsPerSecond = readsPerSecond;
    if (readsPerSecond < minReadsPerSecond) minReadsPerSecond = readsPerSecond;
    
    String status = (errorCount == 0) ? "OK" : "ERROS";
    
    Serial.print(" ");
    if (second < 10) Serial.print(" ");
    Serial.print(second);
    Serial.print(" |    ");
    Serial.print(readsPerSecond);
    Serial.print("    |   ");
    Serial.print(errorCount);
    Serial.print("   | ");
    Serial.println(status);
    
    readsPerSecond = 0;
    lastSecond = millis();
  }
}
