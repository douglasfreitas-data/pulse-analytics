/**
 * ============================================
 * HRV MONITOR v7.1 - RAW DATA COLLECTOR
 * ============================================
 * 
 * VERSÃO SIMPLIFICADA - Apenas coleta e upload
 * - LED Verde @ 400Hz
 * - SEM cálculos de HRV no device
 * - SEM detecção de pico
 * - Coleta raw → Upload Supabase
 * - Processamento feito offline (Python/Server)
 */

#include <Wire.h>
#include <MAX30105.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MAX30105
MAX30105 particleSensor;

// ============================================
// CONFIGURAÇÕES - v7.3 Data-Safe
// ============================================

// Buffer para 30 segundos @ 1000Hz = 30000 amostras (mais seguro para RAM)
const int BUFFER_SIZE = 30000;
uint16_t greenBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Timing
const int SAMPLE_RATE_HZ = 400;  // Começar conservador
const unsigned long SAMPLE_DURATION_MS = 30000; // 30 segundos
unsigned long sampleStartTime = 0;
unsigned long bootTime = 0;

// WiFi
const char* WIFI_SSID = "Freitas";
const char* WIFI_PASS = "2512Jesus";
const char* SUPABASE_URL = "https://pthfxmypcxqjfstqwokf.supabase.co/rest/v1/hrv_sessions";
const char* SUPABASE_KEY = "sb_publishable_V4ZrfeZNld9VROJOWZcE_w_N93BHvqd";

// Sessão
int sessionNumber = 0;
String currentUserName = "Visitante";
String currentSessionTags = "";
int currentSessionAge = 0;
String currentSessionGender = "";

// Estados (SIMPLIFICADO)
enum DeviceState {
  WAITING_BUTTON,    // Esperando botão/comando
  COLLECTING,        // Coletando dados
  UPLOADING          // Enviando para cloud
};
DeviceState currentState = WAITING_BUTTON;

// Detecção de dedo - DESABILITADA (manual via Serial)
// const long FINGER_THRESHOLD = 300;

// ============================================
// WIFI & UPLOAD
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
  display.setCursor(0, 20);
  display.println("Enviando...");
  display.setCursor(0, 35);
  display.print(bufferIndex);
  display.println(" amostras");
  display.display();
  
  if (!client.connect(host, 443)) {
    Serial.println("Erro HTTPS!");
    return;
  }
  
  // Headers
  client.println("POST /rest/v1/hrv_sessions HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Content-Type: application/json");
  client.print("apikey: "); client.println(SUPABASE_KEY);
  client.print("Authorization: Bearer "); client.println(SUPABASE_KEY);
  client.println("Prefer: return=minimal");
  client.println("Transfer-Encoding: chunked");
  client.println("Connection: close");
  client.println();
  
  // JSON - APENAS DADOS RAW
  sendChunk(client, "{");
  sendChunk(client, "\"device_id\": \"ESP32-S3-v7\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  sendChunk(client, "\"sampling_rate_hz\": " + String(SAMPLE_RATE_HZ) + ",");
  sendChunk(client, "\"session_index\": " + String(sessionNumber) + ",");
  sendChunk(client, "\"timestamp_device_min\": " + String((millis() - bootTime) / 60000) + ",");
  
  // Métricas null (serão calculadas offline)
  sendChunk(client, "\"fc_mean\": null,");
  sendChunk(client, "\"sdnn\": null,");
  sendChunk(client, "\"rmssd\": null,");
  sendChunk(client, "\"pnn50\": null,");
  sendChunk(client, "\"rr_valid_count\": null,");
  sendChunk(client, "\"rrr_intervals_ms\": null,");
  sendChunk(client, "\"ir_waveform\": null,");
  sendChunk(client, "\"red_waveform\": null,");
  
  if (currentSessionTags.length() > 0) {
    sendChunk(client, "\"tags\": [\"" + currentSessionTags + "\"],");
  } else {
    sendChunk(client, "\"tags\": null,");
  }
  
  if (currentSessionAge > 0) {
    sendChunk(client, "\"user_age\": " + String(currentSessionAge) + ",");
  } else {
    sendChunk(client, "\"user_age\": null,");
  }
  
  if (currentSessionGender.length() > 0) {
    sendChunk(client, "\"user_gender\": \"" + currentSessionGender + "\",");
  } else {
    sendChunk(client, "\"user_gender\": null,");
  }
  
  // GREEN WAVEFORM - os dados raw!
  sendChunk(client, "\"green_waveform\": [");
  String gBuf = "";
  for (int i = 0; i < bufferIndex; i++) {
    gBuf += String(greenBuffer[i]);
    if (i < bufferIndex - 1) gBuf += ",";
    
    // Flush a cada 200 valores
    if (i % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, gBuf);
      gBuf = "";
      
      // Progress no display
      if (i % 2000 == 0) {
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Enviando...");
        display.setCursor(0, 35);
        int pct = (i * 100) / bufferIndex;
        display.print(pct);
        display.println("%");
        display.display();
      }
    }
  }
  sendChunk(client, "]");
  sendChunk(client, "}");
  
  // End chunks
  client.print("0\r\n\r\n");
  
  // Aguardar resposta
  long timeout = millis();
  while (client.connected() && millis() - timeout < 15000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("HTTP/1.1")) {
        Serial.print("Resposta: "); Serial.println(line);
        if (line.indexOf("201") > 0 || line.indexOf("200") > 0) {
          Serial.println("Upload OK!");
          display.clearDisplay();
          display.setCursor(0, 25);
          display.println("UPLOAD OK!");
          display.display();
        }
      }
      if (line == "\r") break;
      timeout = millis();
    }
  }
  client.stop();
  
  currentSessionTags = "";
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
  
  // Carregar user
  if (SPIFFS.exists("/user_config.txt")) {
    File file = SPIFFS.open("/user_config.txt", "r");
    if (file) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) currentUserName = line;
      file.close();
    }
  }
  
  // Contar sessões
  if (SPIFFS.exists("/session_count.txt")) {
    File file = SPIFFS.open("/session_count.txt", "r");
    if (file) {
      sessionNumber = file.readStringUntil('\n').toInt();
      file.close();
    }
  }
  
  Serial.print("Usuario: "); Serial.println(currentUserName);
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
  
  if (cmd.equalsIgnoreCase("start") || cmd.equalsIgnoreCase("s")) {
    if (currentState == WAITING_BUTTON) {
      startCollection();
    }
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
  else if (cmd.startsWith("AGE:") || cmd.startsWith("age:")) {
    currentSessionAge = cmd.substring(4).toInt();
    Serial.print("Idade: "); Serial.println(currentSessionAge);
  }
  else if (cmd.startsWith("SEX:") || cmd.startsWith("sex:")) {
    currentSessionGender = cmd.substring(4);
    currentSessionGender.trim();
    Serial.print("Sexo: "); Serial.println(currentSessionGender);
  }
  else if (cmd.equalsIgnoreCase("status")) {
    Serial.println("\n=== STATUS ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.println("==============\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS ===");
    Serial.println("start / s    - Iniciar coleta");
    Serial.println("USER:nome    - Definir usuario");
    Serial.println("TAG:tag      - Definir tag da sessao");
    Serial.println("AGE:idade    - Definir idade");
    Serial.println("SEX:m/f      - Definir sexo");
    Serial.println("status       - Ver status");
    Serial.println("================\n");
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
  
  // Mostrar tela inicial de coleta
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Coletando");
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.println("Mantenha o dedo...");
  display.display();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - COLETA INICIADA");
  Serial.println("========================================");
  Serial.println("Mantenha o dedo no sensor por 60s...\n");
}

void handleCollection() {
  // =============================================
  // COLETA DIRETA com check()
  // =============================================
  
  // CRÍTICO: check() atualiza o buffer interno
  particleSensor.check();
  
  if (bufferIndex < BUFFER_SIZE) {
    uint32_t greenValue = particleSensor.getGreen();
    greenBuffer[bufferIndex] = (uint16_t)greenValue;
    bufferIndex++;
  }
  
  // Verificar se terminou
  unsigned long elapsedMS = millis() - sampleStartTime;
  
  if (elapsedMS >= SAMPLE_DURATION_MS) {
    float finalRate = (float)bufferIndex / (SAMPLE_DURATION_MS / 1000.0);
    
    Serial.println("\n========================================");
    Serial.println("   COLETA CONCLUIDA!");
    Serial.print("   Amostras: "); Serial.println(bufferIndex);
    Serial.print("   Taxa Real: "); Serial.print((int)finalRate); Serial.println(" Hz");
    Serial.println("========================================\n");
    
    // Mostrar no display só agora
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("PRONTO!");
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.print("Amostras: ");
    display.println(bufferIndex);
    display.setCursor(0, 40);
    display.print("Taxa: ");
    display.print((int)finalRate);
    display.println(" Hz");
    display.display();
    
    currentState = UPLOADING;
  }
}

void handleUploading() {
  uploadRawData();
  
  delay(3000);
  currentState = WAITING_BUTTON;
  
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("Pronto!");
  display.setCursor(0, 35);
  display.println("Digite 'start'");
  display.display();
}

void showWaitingScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("HRV v7.1 RAW");
  display.setCursor(0, 12);
  display.print("User: ");
  display.println(currentUserName);
  display.setCursor(0, 28);
  display.print("Sessoes: ");
  display.println(sessionNumber);
  display.setCursor(0, 48);
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
  WiFi.disconnect();
  
  Serial.println("\n============================================");
  Serial.println("   HRV MONITOR v7.1 - RAW DATA COLLECTOR");
  Serial.println("============================================");
  Serial.println("Comandos: 'start', 'USER:nome', 'help'");
  Serial.println("============================================\n");
  
  // I2C 400kHz - seguro sem resistores de pull-up
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
  display.println("HRV v7.1 RAW");
  display.setCursor(0, 25);
  display.println("Inicializando...");
  display.display();
  delay(1000);
  
  // MAX30105
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 erro!");
    display.setCursor(0, 40);
    display.println("Sensor ERRO!");
    display.display();
    while (1);
  }
  
  // ============================================
  // CONFIG OTIMIZADA PARA 1000Hz
  // ============================================
  // pulseWidth=69 (15-bit) permite até 3200Hz no sensor
  // adcRange=4096 compensa a resolução menor com mais sensibilidade
  
  byte ledBrightness = 0x1F;  // Brilho moderado
  byte sampleAverage = 1;     // Sem média - crítico para velocidade!
  byte ledMode = 3;           // Multi (obrigatório para Green)
  int sampleRate = 1000;      // TARGET: 1000Hz
  int pulseWidth = 69;        // 15-bit - OBRIGATÓRIO para 1000Hz!
  int adcRange = 4096;        // Mais sensibilidade para compensar pulso curto
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // Red e IR mínimos, Green no máximo
  particleSensor.setPulseAmplitudeRed(0x01);   // Mínimo
  particleSensor.setPulseAmplitudeIR(0x01);    // Mínimo
  particleSensor.setPulseAmplitudeGreen(0x7F); // Alto para sinal forte
  
  particleSensor.clearFIFO();
  
  Serial.println("Sistema pronto!");
  Serial.println("Digite 'start' para iniciar coleta.\n");
  
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
  }
}
