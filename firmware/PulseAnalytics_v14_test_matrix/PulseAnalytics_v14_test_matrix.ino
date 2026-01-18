/**
 * ============================================
 * PULSE ANALYTICS v14.0 - TEST MATRIX
 * ============================================
 * 
 * OBJETIVO: Encontrar a configuração ideal para 800Hz
 * após perda de qualidade por atualização sem backup.
 * 
 * METODOLOGIA:
 * - 12 configurações pré-definidas
 * - Coletas curtas de 10 segundos
 * - Upload separado para cada teste
 * - Tags automáticas para identificação
 * 
 * COMANDOS:
 * - test1 a test12: Executar teste específico
 * - auto: Executar todos os testes sequencialmente
 * - status: Ver configuração atual
 */

#include <Wire.h>
#include <MAX30105.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

// ============================================
// OLED Display
// ============================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// MAX30102 Sensor
// ============================================
MAX30105 particleSensor;

// ============================================
// MATRIZ DE REFINAMENTO PARA 800Hz
// ============================================
// Base: Configuração v11 que funcionou (softReset + Session 18)
// Objetivo: Encontrar o ótimo local variando IR e LED

struct SensorConfig {
  int pulseWidth;      // Fixo em 215 (melhor para 800Hz)
  int adcRange;        // Fixo em 16384 (máximo range)
  byte ledBrightness;  // Variável
  byte irAmplitude;    // Variável
  byte redAmplitude;   // Fixo em 0x7F
  const char* name;    // Nome para tag
};

// 12 configurações de refinamento
const SensorConfig TEST_CONFIGS[] = {
  // Grupo 1: Variando IR Amplitude (LED fixo em 0x7F)
  { 215, 16384, 0x7F, 0x60, 0x7F, "R01_IR60_LED7F" },   // IR baixo
  { 215, 16384, 0x7F, 0x68, 0x7F, "R02_IR68_LED7F" },   // IR intermediário-
  { 215, 16384, 0x7F, 0x70, 0x7F, "R03_IR70_LED7F" },   // ATUAL v11 (referência)
  { 215, 16384, 0x7F, 0x78, 0x7F, "R04_IR78_LED7F" },   // IR intermediário+
  { 215, 16384, 0x7F, 0x80, 0x7F, "R05_IR80_LED7F" },   // IR alto
  
  // Grupo 2: Variando LED Brightness (IR fixo em 0x70)
  { 215, 16384, 0x60, 0x70, 0x7F, "R06_IR70_LED60" },   // LED baixo
  { 215, 16384, 0x70, 0x70, 0x7F, "R07_IR70_LED70" },   // LED médio
  { 215, 16384, 0x90, 0x70, 0x7F, "R08_IR70_LED90" },   // LED alto
  { 215, 16384, 0xA0, 0x70, 0x7F, "R09_IR70_LEDA0" },   // LED muito alto
  
  // Grupo 3: Combinações promissoras
  { 215, 16384, 0x90, 0x68, 0x7F, "R10_IR68_LED90" },   // LED alto + IR baixo
  { 215, 16384, 0x60, 0x78, 0x7F, "R11_IR78_LED60" },   // LED baixo + IR alto
  { 215, 16384, 0x80, 0x75, 0x7F, "R12_IR75_LED80" },   // Equilíbrio otimizado
};

const int NUM_CONFIGS = 12;
int currentTestIndex = -1;  // -1 = nenhum teste ativo

// ============================================
// BUFFER (menor para testes rápidos)
// ============================================
// 10 segundos @ 800Hz = 8.000 amostras
const int BUFFER_SIZE = 10000;
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Timing
const int SAMPLE_RATE_HZ = 800;
const unsigned long SAMPLE_DURATION_MS = 10000; // 10 segundos por teste

unsigned long sampleStartTime = 0;
unsigned long bootTime = 0;

// WiFi & Supabase
const char* WIFI_SSID = "Freitas";
const char* WIFI_PASS = "2512Jesus";
const char* SUPABASE_URL = "https://pthfxmypcxqjfstqwokf.supabase.co/rest/v1/hrv_sessions";
const char* SUPABASE_KEY = "sb_publishable_V4ZrfeZNld9VROJOWZcE_w_N93BHvqd";

// Sessão
int sessionNumber = 0;
String currentUserName = "TestMatrix";
String currentSessionTags = "";
int currentSessionAge = 0;
String currentSessionGender = "";

// Estados
enum DeviceState {
  WAITING_BUTTON,
  COLLECTING,
  UPLOADING,
  UPLOAD_FAIL,
  AUTO_MODE
};
DeviceState currentState = WAITING_BUTTON;

// Auto mode
bool autoModeActive = false;
int autoModeCurrentTest = 0;

// ============================================
// APLICAR CONFIGURAÇÃO DO SENSOR
// ============================================
void applySensorConfig(int testIndex) {
  if (testIndex < 0 || testIndex >= NUM_CONFIGS) {
    Serial.println("Índice de teste inválido!");
    return;
  }
  
  SensorConfig cfg = TEST_CONFIGS[testIndex];
  
  Serial.println("\n========================================");
  Serial.print("   APLICANDO CONFIG: ");
  Serial.println(cfg.name);
  Serial.println("========================================");
  Serial.print("   pulseWidth: "); Serial.println(cfg.pulseWidth);
  Serial.print("   adcRange: "); Serial.println(cfg.adcRange);
  Serial.print("   ledBrightness: 0x"); Serial.println(cfg.ledBrightness, HEX);
  Serial.print("   IR Amplitude: 0x"); Serial.println(cfg.irAmplitude, HEX);
  Serial.print("   Red Amplitude: 0x"); Serial.println(cfg.redAmplitude, HEX);
  Serial.println("========================================\n");
  
  // Configurar sensor
  byte sampleAverage = 1;  // Sempre 1 para 800Hz
  byte ledMode = 2;        // Red + IR
  int sampleRate = 800;    // Fixo em 800Hz
  
  particleSensor.setup(cfg.ledBrightness, sampleAverage, ledMode, sampleRate, cfg.pulseWidth, cfg.adcRange);
  particleSensor.setPulseAmplitudeRed(cfg.redAmplitude);
  particleSensor.setPulseAmplitudeIR(cfg.irAmplitude);
  particleSensor.clearFIFO();
  
  // Definir tag automática
  currentSessionTags = String(cfg.name);
  currentTestIndex = testIndex;
  
  // Mostrar no display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("CONFIG APLICADA:");
  display.setCursor(0, 15);
  display.println(cfg.name);
  display.setCursor(0, 30);
  display.print("PW:"); display.print(cfg.pulseWidth);
  display.print(" ADC:"); display.println(cfg.adcRange);
  display.setCursor(0, 45);
  display.print("LED:0x"); display.print(cfg.ledBrightness, HEX);
  display.print(" IR:0x"); display.println(cfg.irAmplitude, HEX);
  display.display();
  
  delay(1500);
}

// ============================================
// WiFi & UPLOAD
// ============================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("Conectando WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
  } else {
    Serial.println(" FALHA!");
  }
}

void sendChunk(NetworkClientSecure &client, String data) {
  client.print(data.length(), HEX);
  client.println();
  client.print(data);
  client.println();
}

void uploadRawData() {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi - dados perdidos!");
    return;
  }
  
  NetworkClientSecure client;
  client.setInsecure();
  
  const char* host = "pthfxmypcxqjfstqwokf.supabase.co";
  
  Serial.println("Conectando Supabase...");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Enviando...");
  display.setCursor(0, 25);
  display.print(bufferIndex);
  display.println(" amostras");
  display.setCursor(0, 40);
  display.println(currentSessionTags);
  display.display();
  
  Serial.println(ESP.getFreeHeap());
  
  client.setTimeout(30000); // 30s timeout (dados menores)

  if (!client.connect(host, 443)) {
    Serial.print("Erro HTTPS! Heap: ");
    Serial.println(ESP.getFreeHeap());
    display.setCursor(0, 55);
    display.println("ERRO HTTPS!");
    display.display();
    currentState = UPLOAD_FAIL;
    return;
  }
  
  // Headers HTTP
  client.println("POST /rest/v1/hrv_sessions HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Content-Type: application/json");
  client.print("apikey: "); client.println(SUPABASE_KEY);
  client.print("Authorization: Bearer "); client.println(SUPABASE_KEY);
  client.println("Prefer: return=minimal");
  client.println("Transfer-Encoding: chunked");
  client.println("Connection: close");
  client.println();
  
  // JSON
  sendChunk(client, "{");
  sendChunk(client, "\"device_id\": \"ESP32-S3-v14-TestMatrix\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  sendChunk(client, "\"sampling_rate_hz\": " + String(SAMPLE_RATE_HZ) + ",");
  sendChunk(client, "\"session_index\": " + String(sessionNumber) + ",");
  sendChunk(client, "\"timestamp_device_min\": " + String((millis() - bootTime) / 60000) + ",");
  
  // Métricas null
  sendChunk(client, "\"fc_mean\": null,");
  sendChunk(client, "\"sdnn\": null,");
  sendChunk(client, "\"rmssd\": null,");
  sendChunk(client, "\"pnn50\": null,");
  sendChunk(client, "\"rr_valid_count\": null,");
  sendChunk(client, "\"rrr_intervals_ms\": null,");
  sendChunk(client, "\"green_waveform\": null,");
  
  // Tags (nome da configuração)
  sendChunk(client, "\"tags\": [\"" + currentSessionTags + "\"],");
  
  // Demographics
  sendChunk(client, "\"user_age\": null,");
  sendChunk(client, "\"user_gender\": null,");
  
  // IR WAVEFORM
  Serial.println("Enviando IR waveform...");
  sendChunk(client, "\"ir_waveform\": [");
  String irBuf = "";
  for (int i = 0; i < bufferIndex; i++) {
    irBuf += String(irBuffer[i]);
    if (i < bufferIndex - 1) irBuf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, irBuf);
      irBuf = "";
      yield();
      
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload IR!");
        currentState = UPLOAD_FAIL;
        return;
      }
      
      if ((i + 1) % 2000 == 0) {
        int pct = (i * 50) / bufferIndex;
        Serial.print("IR: "); Serial.print(pct); Serial.println("%");
      }
    }
  }
  sendChunk(client, "],");
  Serial.println("IR OK!");
  
  // RED WAVEFORM
  Serial.println("Enviando Red waveform...");
  sendChunk(client, "\"red_waveform\": [");
  String redBuf = "";
  for (int i = 0; i < bufferIndex; i++) {
    redBuf += String(redBuffer[i]);
    if (i < bufferIndex - 1) redBuf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, redBuf);
      redBuf = "";
      yield();
      
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload RED!");
        currentState = UPLOAD_FAIL;
        return;
      }
      
      if ((i + 1) % 2000 == 0) {
        int pct = 50 + (i * 50) / bufferIndex;
        Serial.print("Red: "); Serial.print(pct); Serial.println("%");
      }
    }
  }
  sendChunk(client, "]");
  Serial.println("Red OK!");
  sendChunk(client, "}");
  
  // End chunks
  client.print("0\r\n\r\n");
  
  // Aguardar resposta
  long timeout = millis();
  bool success = false;
  while (client.connected() && millis() - timeout < 30000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("HTTP/1.1")) {
        Serial.print("Resposta: "); Serial.println(line);
        if (line.indexOf("201") > 0 || line.indexOf("200") > 0) {
          success = true;
          Serial.println("Upload OK!");
        }
      }
      if (line == "\r") break;
      timeout = millis();
    }
  }
  client.stop();
  
  // Resultado
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  if (success) {
    display.println("SUCESSO!");
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.println(currentSessionTags);
  } else {
    display.println("ERRO!");
  }
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print(bufferIndex);
  display.println(" amostras");
  display.display();
  
  if (success) {
    if (autoModeActive) {
      currentState = AUTO_MODE;
    } else {
      currentState = WAITING_BUTTON;
    }
  } else {
    currentState = UPLOAD_FAIL;
  }
}

// ============================================
// SPIFFS
// ============================================
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro SPIFFS!");
    return;
  }
  Serial.println("SPIFFS OK");
  
  // Contar sessões
  if (SPIFFS.exists("/session_count.txt")) {
    File file = SPIFFS.open("/session_count.txt", "r");
    if (file) {
      sessionNumber = file.readStringUntil('\n').toInt();
      file.close();
    }
  }
  
  Serial.print("Sessoes: "); Serial.println(sessionNumber);
}

void saveSessionCount() {
  File file = SPIFFS.open("/session_count.txt", "w");
  if (file) {
    file.println(sessionNumber);
    file.close();
  }
}

// ============================================
// COMANDOS SERIAL
// ============================================
void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  
  // Testes individuais: test1 a test12
  if (cmd.startsWith("test")) {
    int testNum = cmd.substring(4).toInt();
    if (testNum >= 1 && testNum <= NUM_CONFIGS) {
      applySensorConfig(testNum - 1);
      startCollection();
    } else {
      Serial.println("Teste inválido! Use test1 a test12");
    }
  }
  // Atalhos t1 a t12
  else if (cmd.length() >= 2 && cmd.charAt(0) == 't' && isDigit(cmd.charAt(1))) {
    int testNum = cmd.substring(1).toInt();
    if (testNum >= 1 && testNum <= NUM_CONFIGS) {
      applySensorConfig(testNum - 1);
      startCollection();
    }
  }
  // Auto mode
  else if (cmd == "auto" || cmd == "a") {
    Serial.println("\n========================================");
    Serial.println("   MODO AUTOMÁTICO INICIADO!");
    Serial.println("   Executando todos os 12 testes...");
    Serial.println("========================================\n");
    autoModeActive = true;
    autoModeCurrentTest = 0;
    currentState = AUTO_MODE;
  }
  // Configurações
  else if (cmd == "configs" || cmd == "c") {
    Serial.println("\n========================================");
    Serial.println("   MATRIZ DE CONFIGURAÇÕES (800Hz)");
    Serial.println("========================================");
    for (int i = 0; i < NUM_CONFIGS; i++) {
      Serial.print(i + 1); Serial.print(". ");
      Serial.print(TEST_CONFIGS[i].name);
      Serial.print(" | PW:"); Serial.print(TEST_CONFIGS[i].pulseWidth);
      Serial.print(" ADC:"); Serial.print(TEST_CONFIGS[i].adcRange);
      Serial.print(" LED:0x"); Serial.print(TEST_CONFIGS[i].ledBrightness, HEX);
      Serial.print(" IR:0x"); Serial.println(TEST_CONFIGS[i].irAmplitude, HEX);
    }
    Serial.println("========================================\n");
  }
  // Retry
  else if (cmd == "retry" || cmd == "r") {
    if (currentState == UPLOAD_FAIL) {
      Serial.println("Retentando upload...");
      currentState = UPLOADING;
    }
  }
  // Help
  else if (cmd == "help" || cmd == "h") {
    Serial.println("\n========================================");
    Serial.println("   COMANDOS v14 - TEST MATRIX");
    Serial.println("========================================");
    Serial.println("test1-test12  Executar teste específico");
    Serial.println("t1-t12        Atalho para testes");
    Serial.println("auto / a      Executar todos os testes");
    Serial.println("configs / c   Listar configurações");
    Serial.println("retry / r     Reenviar upload falho");
    Serial.println("help / h      Mostrar ajuda");
    Serial.println("========================================\n");
  }
}

// ============================================
// COLETA DE DADOS
// ============================================
void startCollection() {
  sessionNumber++;
  saveSessionCount();
  
  bufferIndex = 0;
  particleSensor.clearFIFO();
  
  currentState = COLLECTING;
  sampleStartTime = millis();
  
  Serial.println("\n========================================");
  Serial.print("   TESTE "); Serial.print(currentTestIndex + 1);
  Serial.print("/"); Serial.print(NUM_CONFIGS);
  Serial.println(" - COLETA INICIADA");
  Serial.println("========================================");
  Serial.print("Config: "); Serial.println(currentSessionTags);
  Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" segundos");
  Serial.println("Mantenha o dedo no sensor...\n");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("TESTE "); display.print(currentTestIndex + 1);
  display.print("/"); display.println(NUM_CONFIGS);
  display.setCursor(0, 15);
  display.println(currentSessionTags);
  display.setCursor(0, 35);
  display.println("Mantenha o dedo...");
  display.setCursor(0, 50);
  display.print("10 segundos");
  display.display();
}

void handleCollection() {
  // Leitura do FIFO
  while (particleSensor.available()) {
    if (bufferIndex < BUFFER_SIZE) {
      uint32_t irValue = particleSensor.getFIFOIR();
      uint32_t redValue = particleSensor.getFIFORed();
      
      irBuffer[bufferIndex] = (uint16_t)irValue;
      redBuffer[bufferIndex] = (uint16_t)redValue;
      bufferIndex++;
      
      particleSensor.nextSample();
    } else {
      particleSensor.nextSample();
    }
  }
  
  particleSensor.check();
  
  // Atualizar display a cada segundo
  unsigned long elapsedMS = millis() - sampleStartTime;
  static unsigned long lastDisplayUpdate = 0;
  
  if (elapsedMS - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = elapsedMS;
    
    int secondsRemaining = (SAMPLE_DURATION_MS - elapsedMS) / 1000;
    float currentRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(50, 5);
    display.print(secondsRemaining);
    display.println("s");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Amostras: "); display.println(bufferIndex);
    display.setCursor(0, 42);
    display.print("Taxa: "); display.print((int)currentRate); display.println(" Hz");
    display.setCursor(0, 54);
    display.println(currentSessionTags);
    display.display();
    
    Serial.print("["); Serial.print(elapsedMS/1000); Serial.print("s] ");
    Serial.print("Amostras: "); Serial.print(bufferIndex);
    Serial.print(" | Taxa: "); Serial.print((int)currentRate); Serial.println(" Hz");
  }
  
  // Verificar se terminou
  if (elapsedMS >= SAMPLE_DURATION_MS || bufferIndex >= BUFFER_SIZE) {
    float finalRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    
    Serial.println("\n========================================");
    Serial.println("   COLETA CONCLUIDA!");
    Serial.print("   Config: "); Serial.println(currentSessionTags);
    Serial.print("   Amostras: "); Serial.println(bufferIndex);
    Serial.print("   Taxa Real: "); Serial.print(finalRate, 1); Serial.println(" Hz");
    Serial.println("========================================\n");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 5);
    display.println("COLETA OK!");
    display.setCursor(0, 20);
    display.println(currentSessionTags);
    display.setCursor(0, 35);
    display.print("Amostras: "); display.println(bufferIndex);
    display.setCursor(0, 50);
    display.println("Enviando...");
    display.display();
    
    delay(500);
    currentState = UPLOADING;
  }
}

void handleUploading() {
  uploadRawData();
  
  if (currentState == WAITING_BUTTON) {
    delay(2000);
    showWaitingScreen();
  } else if (currentState == UPLOAD_FAIL) {
    Serial.println("Falha no Upload. Digite 'retry' para tentar novamente.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("FALHA UPLOAD!");
    display.setCursor(0, 30);
    display.println(currentSessionTags);
    display.setCursor(0, 50);
    display.println("Digite 'retry'");
    display.display();
  }
}

void handleAutoMode() {
  if (autoModeCurrentTest >= NUM_CONFIGS) {
    // Todos os testes concluídos
    Serial.println("\n========================================");
    Serial.println("   MODO AUTOMÁTICO CONCLUÍDO!");
    Serial.println("   Todos os 12 testes foram executados.");
    Serial.println("========================================\n");
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.println("COMPLETO!");
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.println("12/12 testes OK");
    display.setCursor(0, 50);
    display.println("Verifique Supabase");
    display.display();
    
    autoModeActive = false;
    currentState = WAITING_BUTTON;
    return;
  }
  
  Serial.print("\n>>> Auto Mode: Iniciando teste ");
  Serial.print(autoModeCurrentTest + 1);
  Serial.print("/");
  Serial.println(NUM_CONFIGS);
  
  applySensorConfig(autoModeCurrentTest);
  autoModeCurrentTest++;
  startCollection();
}

void showWaitingScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("PulseAnalytics v14");
  display.setCursor(0, 12);
  display.println("TEST MATRIX 800Hz");
  display.setCursor(0, 28);
  display.println("Comandos:");
  display.setCursor(0, 40);
  display.println("t1-t12, auto, help");
  display.setCursor(0, 54);
  display.print("Sessoes: ");
  display.println(sessionNumber);
  display.display();
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  bootTime = millis();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.println("\n============================================");
  Serial.println("   PULSE ANALYTICS v14.0 - TEST MATRIX");
  Serial.println("   Matriz de testes para 800Hz");
  Serial.println("============================================");
  Serial.println("Comandos: test1-12, auto, configs, help");
  Serial.println("============================================\n");
  
  // I2C 400kHz
  Wire.begin();
  Wire.setClock(400000);
  
  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED erro!");
    while (1);
  }
  
  // SPIFFS
  initSPIFFS();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("PulseAnalytics v14");
  display.setCursor(0, 25);
  display.println("TEST MATRIX");
  display.setCursor(0, 45);
  display.println("Inicializando...");
  display.display();
  delay(1000);
  
  // MAX30102 - Config inicial (T10 = Session 18)
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 erro!");
    display.setCursor(0, 55);
    display.println("Sensor ERRO!");
    display.display();
    while (1);
  }
  
  // ============================================
  // FAXINA: Limpar estado do sensor
  // ============================================
  Serial.println("Executando softReset()...");
  particleSensor.softReset();
  delay(500);  // Tempo para o sensor se recuperar
  Serial.println("Sensor resetado!");
  
  // Aplicar config padrão (v11 que funcionou)
  applySensorConfig(2);  // R03 = índice 2 (referência)
  
  Serial.println("\nSistema pronto!");
  Serial.println("Digite 'help' para ver comandos.\n");
  
  currentState = WAITING_BUTTON;
}

// ============================================
// LOOP
// ============================================
void loop() {
  // Comandos Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  
  switch (currentState) {
    case WAITING_BUTTON:
      showWaitingScreen();
      break;
      
    case COLLECTING:
      handleCollection();
      break;
      
    case UPLOADING:
      handleUploading();
      break;

    case UPLOAD_FAIL:
      // Fica parado esperando comando 'retry'
      break;
      
    case AUTO_MODE:
      handleAutoMode();
      break;
  }
}
