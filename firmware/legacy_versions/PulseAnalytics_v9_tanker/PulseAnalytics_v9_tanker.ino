/**
 * ============================================
 * PULSE ANALYTICS v9.1 - "THE TANKER 400Hz"
 * ============================================
 * 
 * ESTRATÉGIA: Multi-Core Batching
 * - Core 1: Coleta de dados (alta prioridade)
 * - Core 0: Upload WiFi (background)
 * 
 * SOLUÇÃO PARA TRUNCAMENTO:
 * - Envia em lotes de 4000 amostras (~5 segundos)
 * - Cada lote é uma linha separada no banco
 * - session_uuid agrupa todos os lotes
 * 
 * SENSOR: MAX30102 (Red + IR)
 * TAXA: 400 Hz (pulseWidth 411µs)
 * DURAÇÃO: 60 segundos (teste)
 */

#include <Wire.h>
#include <MAX30105.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

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
// CONFIGURAÇÕES v9.0 "THE TANKER"
// ============================================

// Batch configuration - TESTE 400Hz: 60 segundos
const int BATCH_SIZE = 2000;           // ~5 segundos @ 400Hz
const int MAX_BATCHES = 12;            // 12 batches = 60 segundos
const int SAMPLE_RATE_HZ = 400;
const unsigned long TOTAL_DURATION_MS = 60000;  // 60 segundos (TESTE)

// Buffers para coleta (Tanker Mode - Unico Buffer Gigante)
// 60s * 400Hz = 24.000 samples per channel
// 24.000 * 2 bytes = 48KB per channel -> 96KB Total
// ESP32-S3 tem ~300KB livres
uint16_t* bigIrBuffer = NULL;
uint16_t* bigRedBuffer = NULL;
int collectedSamples = 0;

// Controle de sessão
String sessionUUID = "";
int currentBatch = 0;
int totalSamplesSent = 0;
bool collectionActive = false;
unsigned long collectionStartTime = 0;

// Struct com ponteiros para os buffers (não copia dados)
struct BatchInfo {
  uint16_t* ir;              // Ponteiro para buffer IR
  uint16_t* red;             // Ponteiro para buffer Red
  int count;
  int batchNumber;
  unsigned long startMillis;
  unsigned long endMillis;
  bool needsFree;            // Se precisa liberar memória após upload
};

// WiFi & Supabase
const char* WIFI_SSID = "Freitas";
const char* WIFI_PASS = "2512Jesus";
const char* SUPABASE_URL = "https://pthfxmypcxqjfstqwokf.supabase.co/rest/v1/hrv_batches";
const char* SUPABASE_KEY = "sb_publishable_V4ZrfeZNld9VROJOWZcE_w_N93BHvqd";

// Sessão
int sessionNumber = 0;
String currentUserName = "Visitante";
String currentSessionTags = "";
int currentSessionAge = 0;
String currentSessionGender = "";
unsigned long bootTime = 0;

// Estados
enum DeviceState {
  WAITING_BUTTON,
  COLLECTING,
  UPLOADING,
  FINISHING
};
volatile DeviceState currentState = WAITING_BUTTON;

// ============================================
// GERAR UUID SIMPLES
// ============================================
String generateUUID() {
  String uuid = "";
  for (int i = 0; i < 32; i++) {
    if (i == 8 || i == 12 || i == 16 || i == 20) uuid += "-";
    uuid += String(random(16), HEX);
  }
  return uuid;
}

// ============================================
// WIFI
// ============================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("WiFi: ");
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

// ============================================
// UPLOAD TASK (Core 0)
// ============================================
void sendChunk(NetworkClientSecure &client, String data) {
  client.print(data.length(), HEX);
  client.println();
  client.print(data);
  client.println();
}

bool uploadBatch(BatchInfo* batch) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return false;
  }
  
  NetworkClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  
  const char* host = "pthfxmypcxqjfstqwokf.supabase.co";
  
  if (!client.connect(host, 443)) {
    Serial.println("HTTPS falhou!");
    return false;
  }
  
  // Headers
  client.println("POST /rest/v1/hrv_batches HTTP/1.1");
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
  sendChunk(client, "\"session_uuid\": \"" + sessionUUID + "\",");
  sendChunk(client, "\"batch_number\": " + String(batch->batchNumber) + ",");
  sendChunk(client, "\"sample_count\": " + String(batch->count) + ",");
  sendChunk(client, "\"start_millis\": " + String(batch->startMillis) + ",");
  sendChunk(client, "\"end_millis\": " + String(batch->endMillis) + ",");
  sendChunk(client, "\"sampling_rate_hz\": " + String(SAMPLE_RATE_HZ) + ",");
  sendChunk(client, "\"device_id\": \"ESP32-S3-v9.1-Tanker-400Hz\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  
  // IR Waveform
  sendChunk(client, "\"ir_waveform\": [");
  String buf = "";
  for (int i = 0; i < batch->count; i++) {
    buf += String(batch->ir[i]);
    if (i < batch->count - 1) buf += ",";
    if ((i + 1) % 200 == 0 || i == batch->count - 1) {
      sendChunk(client, buf);
      buf = "";
      yield();
    }
  }
  sendChunk(client, "],");
  
  // Red Waveform
  sendChunk(client, "\"red_waveform\": [");
  buf = "";
  for (int i = 0; i < batch->count; i++) {
    buf += String(batch->red[i]);
    if (i < batch->count - 1) buf += ",";
    if ((i + 1) % 200 == 0 || i == batch->count - 1) {
      sendChunk(client, buf);
      buf = "";
      yield();
    }
  }
  sendChunk(client, "]");
  sendChunk(client, "}");
  
  client.print("0\r\n\r\n");
  
  // Aguardar resposta
  bool success = false;
  long timeout = millis();
  while (client.connected() && millis() - timeout < 10000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("201") > 0) {
          success = true;
        }
      }
      if (line == "\r") break;
      timeout = millis();
    }
    yield();
  }
  client.stop();
  
  return success;
}



// ============================================
// SPIFFS
// ============================================
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro SPIFFS!");
    return;
  }
  
  if (SPIFFS.exists("/user_config.txt")) {
    File file = SPIFFS.open("/user_config.txt", "r");
    if (file) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) currentUserName = line;
      file.close();
    }
  }
  
  if (SPIFFS.exists("/session_count.txt")) {
    File file = SPIFFS.open("/session_count.txt", "r");
    if (file) {
      sessionNumber = file.readStringUntil('\n').toInt();
      file.close();
    }
  }
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
  
  if (cmd.equalsIgnoreCase("start") || cmd.equalsIgnoreCase("s")) {
    if (currentState == WAITING_BUTTON) {
      startCollection();
    }
  }
  else if (cmd.equalsIgnoreCase("c")) {
    sessionNumber = 0;
    saveSessionCount();
    Serial.println("Sessoes resetadas!");
  }
  else if (cmd.equalsIgnoreCase("l")) {
    Serial.println("\n=== STATUS v9.0 ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Batch Size: "); Serial.println(BATCH_SIZE);
    Serial.print("Duracao: "); Serial.print(TOTAL_DURATION_MS/1000); Serial.println("s");
    Serial.println("===================\n");
  }
  else if (cmd.startsWith("USER:") || cmd.startsWith("user:")) {
    currentUserName = cmd.substring(5);
    currentUserName.trim();
    File file = SPIFFS.open("/user_config.txt", "w");
    if (file) { file.println(currentUserName); file.close(); }
    Serial.print("Usuario: "); Serial.println(currentUserName);
  }
  else if (cmd.startsWith("TAG:") || cmd.startsWith("tag:")) {
    currentSessionTags = cmd.substring(4);
    currentSessionTags.trim();
    Serial.print("Tag: "); Serial.println(currentSessionTags);
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== v9.0 TANKER ===");
    Serial.println("start/s - Iniciar (5 min)");
    Serial.println("c       - Reset sessoes");
    Serial.println("l       - Status");
    Serial.println("USER:x  - Usuario");
    Serial.println("TAG:x   - Tag");
    Serial.println("===================\n");
  }
}

// ============================================
// COLETA DE DADOS
// ============================================
// ============================================
// COLETA DE DADOS
// ============================================
void startCollection() {
  sessionNumber++;
  saveSessionCount();
  
  sessionUUID = generateUUID();
  currentBatch = 0;
  totalSamplesSent = 0;
  collectedSamples = 0;
  
  // Alocar BUFFER PRE-DEMO (Single Giant Buffer)
  if (bigIrBuffer != NULL) free(bigIrBuffer);
  if (bigRedBuffer != NULL) free(bigRedBuffer);
  
  // 60s * 400Hz = 24000 samples
  int totalSamples = 24000;
  
  bigIrBuffer = (uint16_t*)calloc(totalSamples, sizeof(uint16_t));
  bigRedBuffer = (uint16_t*)calloc(totalSamples, sizeof(uint16_t));
  
  if (bigIrBuffer == NULL || bigRedBuffer == NULL) {
    Serial.println("ERRO FATAL: Sem RAM para Tanker Mode!");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("ERRO RAM!");
    display.display();
    return;
  }
  
  particleSensor.clearFIFO();
  
  currentState = COLLECTING;
  collectionActive = true;
  collectionStartTime = millis();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - v9.1 TANKER 400Hz (TRUE MODE)");
  Serial.println("========================================");
  Serial.print("UUID: "); Serial.println(sessionUUID);
  Serial.print("Duracao: "); Serial.print(TOTAL_DURATION_MS/1000); Serial.println("s");
  Serial.println("Mantenha o dedo no sensor...\n");
  
  // NAO CONECTAR WIFI AGORA!
}

void handleCollection() {
  // Ler novos dados do sensor para o buffer interno da lib
  particleSensor.check();

  // 60s * 400Hz = 24000 samples
  int MAX_SAMPLES = 24000;
  
  // Ler FIFO do sensor
  while (particleSensor.available()) {
    if (collectedSamples < MAX_SAMPLES) {
      bigIrBuffer[collectedSamples] = (uint16_t)particleSensor.getFIFOIR();
      bigRedBuffer[collectedSamples] = (uint16_t)particleSensor.getFIFORed();
      collectedSamples++;
      particleSensor.nextSample();
    } else {
      // Buffer cheio - acabou coleta
      particleSensor.nextSample(); // Descartar extras
    }
  }
  
  // Atualizar display
  unsigned long elapsed = millis() - collectionStartTime;
  static unsigned long lastDisplay = 0;
  
  if (elapsed - lastDisplay >= 1000) {
    lastDisplay = elapsed;
    
    int secondsRemaining = (TOTAL_DURATION_MS - elapsed) / 1000;
    if (secondsRemaining < 0) secondsRemaining = 0;
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 0);
    display.print(secondsRemaining);
    display.println("s");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Samples: "); display.println(collectedSamples);
    display.setCursor(0, 45);
    display.println("Collecting...");
    display.display();
    
    // Log
    Serial.print("["); Serial.print(elapsed/1000); Serial.print("s] ");
    Serial.print("Samples: "); Serial.println(collectedSamples);
  }
  
  // Verificar fim
  if (elapsed >= TOTAL_DURATION_MS || collectedSamples >= MAX_SAMPLES) {
    finishCollection();
  }
}



void finishCollection() {
  collectionActive = false;
  
  unsigned long elapsed = millis() - collectionStartTime;
  
  Serial.println("\n========================================");
  Serial.println("   COLETA CONCLUIDA! (Saving to RAM)");
  Serial.print("   Total Samples: "); Serial.println(collectedSamples);
  Serial.print("   Tempo: "); Serial.print(elapsed/1000); Serial.println("s");
  Serial.println("========================================\n");
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println("SAVED!");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println("Connecting WiFi...");
  display.display();
  
  // Mudar estado para UPLOAD
  currentState = UPLOADING;
}

void showWaitingScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("PulseAnalytics v9.1");
  display.setCursor(0, 12);
  display.println("TANKER 400Hz");
  display.setCursor(0, 28);
  display.print("User: "); display.println(currentUserName);
  display.setCursor(0, 40);
  display.print("Sessions: "); display.println(sessionNumber);
  display.setCursor(0, 54);
  display.println("Serial: 'start'");
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
  
  Serial.println("\n============================================");
  Serial.println("   PULSE ANALYTICS v9.1 - TANKER 400Hz");
  Serial.println("   Multi-Core Batching @ 400Hz");
  Serial.println("============================================");
  Serial.println("Batch: 2000 samples (~5s) | PulseWidth: 411us");
  Serial.println("============================================\n");
  
  Serial.println("Batch: 2000 samples (~5s) | PulseWidth: 411us");
  Serial.println("============================================\n");
  
  // I2C
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
  display.println("v9.1 TANKER 400Hz");
  display.setCursor(0, 30);
  display.println("Inicializando...");
  display.display();
  delay(1000);
  
  // Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor erro!");
    while (1);
  }
  
  // Config sensor para 400Hz (pulseWidth padrão)
  byte ledBrightness = 0x3F;
  byte sampleAverage = 1;
  byte ledMode = 2;
  int sampleRate = 400;
  int pulseWidth = 411;
  int adcRange = 16384;
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x7F);
  particleSensor.setPulseAmplitudeIR(0x7F);
  particleSensor.clearFIFO();
  
  Serial.println("Sistema pronto!");
  Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
  Serial.println("Comandos: 'start', 'help'\n");
  
  currentState = WAITING_BUTTON;
}

void performUpload() {
  // 1. Conectar WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro WiFi. Abortando upload.");
    currentState = WAITING_BUTTON;
    return;
  }
  
  // 2. Preparar batches
  int totalSamples = collectedSamples;
  int UPLOAD_BATCH_SIZE = 2000;
  int numBatches = (totalSamples + UPLOAD_BATCH_SIZE - 1) / UPLOAD_BATCH_SIZE;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("UPLOADING...");
  display.display();
  
  // 3. Loop de Upload
  for (int i = 0; i < numBatches; i++) {
    int startIdx = i * UPLOAD_BATCH_SIZE;
    int count = UPLOAD_BATCH_SIZE;
    if (startIdx + count > totalSamples) count = totalSamples - startIdx;
    
    // Configurar batch temporário apontando para o buffer gigante
    // Nao precisamos da struct complexa, vamos chamar direto ou adaptar
    // Mas para manter compatibilidade com a funcao de upload existente, vamos criar um fake batch
    
    BatchInfo fakeBatch;
    fakeBatch.ir = &bigIrBuffer[startIdx]; // Pointer arithmetic
    fakeBatch.red = &bigRedBuffer[startIdx];
    fakeBatch.count = count;
    fakeBatch.batchNumber = i;
    // Timestamps aproximados (nao temos per-batch timestamp na memoria, vamos estimar ou usar 0)
    // 400Hz = 2.5ms por sample
    fakeBatch.startMillis = startIdx * 2.5; 
    fakeBatch.endMillis = (startIdx + count) * 2.5;
    fakeBatch.needsFree = false; // Importante!
    
    Serial.print("Uploading Batch "); Serial.print(i+1); Serial.print("/"); Serial.println(numBatches);
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Upload "); display.print(i+1); display.print("/"); display.println(numBatches);
    display.display();
    
    bool success = uploadBatch(&fakeBatch);
    
    if (!success) {
      Serial.println("Falha no batch. Tentando novamente...");
      delay(1000);
      success = uploadBatch(&fakeBatch); // Retry simples
      if (!success) Serial.println("Batch perdido.");
    }
  }
  
  Serial.println("Upload Concluido!");
  
  // 4. Limpeza
  // Opcional: Desconectar WiFi para economizar bateria?
  // WiFi.disconnect();
  
  // Free buffers? Nao, mantenha alocado para proxima, ou free se quiser economizar
  // Vamos manter para evitar fragmentacao
  
  currentState = WAITING_BUTTON;
}

// ============================================
// LOOP (Core 1)
// ============================================
void loop() {
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
      performUpload();
      break;
      
    case FINISHING:
      break;
  }
}
