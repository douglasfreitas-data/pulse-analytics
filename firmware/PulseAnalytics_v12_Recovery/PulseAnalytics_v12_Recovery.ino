/**
 * ============================================
 * PULSE ANALYTICS v8.0 - CLOUD STORAGE
 * ============================================
 * 
 * SENSOR: MAX30102 (Red + IR apenas, sem Green)
 * FOCO: Coleta de alta frequência para análise offline
 * 
 * Configuração:
 * - LED Mode: 2 (Red + IR)
 * - Sample Rate: 400Hz
 * - Pulse Width: 69μs (15-bit ADC)
 * - Sample Average: 1 (sem média)
 * - I2C: 400kHz Fast Mode
 * 
 * Estratégia:
 * - Buffer local para 300 segundos
 * - Upload único no final (mais seguro para WiFi instável)
 * - Sem cálculos de BPM/SpO2 (feito offline)
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
// CONFIGURAÇÕES v8.0
// ============================================

// Buffer para 300 segundos @ 400Hz = 120.000 amostras
// ATENÇÃO: ESP32-S3 tem ~320KB de RAM disponível
// 2 buffers x 120000 x 2 bytes = 480KB (MUITO!)
// Solução: usar buffer menor ou compressão

// Buffer otimizado: 60 segundos @ 400Hz = 24.000 amostras
// Depois de testar a taxa, podemos aumentar gradualmente
// Buffer otimizado: 50 segundos @ 800Hz = 40.000 amostras
// RAM Req: 40k * 2ch * 2 bytes = 160KB. (Libera 32KB para SSL)
const int BUFFER_SIZE = 40000;
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Timing
const int SAMPLE_RATE_HZ = 800;
const unsigned long SAMPLE_INTERVAL_US = 1250;  // 1000000 / 800 = 1250μs
const unsigned long SAMPLE_DURATION_MS = 50000; // 50 segundos (Max RAM p/ SSL)
// TODO: Aumentar para 300000 (5 min) após validar taxa

unsigned long sampleStartTime = 0;
unsigned long bootTime = 0;

// WiFi & Supabase
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

// Estados
enum DeviceState {
  WAITING_BUTTON,
  COLLECTING,
  UPLOADING
};
DeviceState currentState = WAITING_BUTTON;

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
  display.println("Aguarde...");
  display.display();
  
  if (!client.connect(host, 443)) {
    Serial.println("Erro HTTPS!");
    display.setCursor(0, 55);
    display.println("ERRO HTTPS!");
    display.display();
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
  
  // JSON - Estrutura simplificada para v8.0
  sendChunk(client, "{");
  sendChunk(client, "\"device_id\": \"ESP32-S3-v12-Recovery-800Hz\",");
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
  sendChunk(client, "\"green_waveform\": null,");  // MAX30102 não tem Green
  
  // Tags
  if (currentSessionTags.length() > 0) {
    sendChunk(client, "\"tags\": [\"" + currentSessionTags + "\"],");
  } else {
    sendChunk(client, "\"tags\": null,");
  }
  
  // Demographics
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
  
  // IR WAVEFORM
  Serial.println("Enviando IR waveform...");
  sendChunk(client, "\"ir_waveform\": [");
  String irBuf = "";
  for (int i = 0; i < bufferIndex; i++) {
    irBuf += String(irBuffer[i]);
    if (i < bufferIndex - 1) irBuf += ",";
    
    // Flush buffer a cada 200 valores
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, irBuf);
      irBuf = "";
      
      // CRÍTICO: yield() para evitar watchdog reset
      yield();
      
      // Verificar se conexão ainda está ativa
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload IR!");
        return;
      }
      
      // Progress a cada 2000 valores
      if ((i + 1) % 2000 == 0) {
        int pct = (i * 50) / bufferIndex;
        Serial.print("IR: "); Serial.print(pct); Serial.println("%");
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Enviando IR...");
        display.setCursor(0, 35);
        display.print(pct); display.println("%");
        display.display();
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
    
    // Flush buffer a cada 200 valores
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, redBuf);
      redBuf = "";
      
      // CRÍTICO: yield() para evitar watchdog reset
      yield();
      
      // Verificar se conexão ainda está ativa
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload Red!");
        return;
      }
      
      // Progress a cada 2000 valores
      if ((i + 1) % 2000 == 0) {
        int pct = 50 + (i * 50) / bufferIndex;
        Serial.print("Red: "); Serial.print(pct); Serial.println("%");
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Enviando Red...");
        display.setCursor(0, 35);
        display.print(pct); display.println("%");
        display.display();
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
  display.setCursor(10, 20);
  if (success) {
    display.println("SUCESSO!");
  } else {
    display.println("ERRO!");
  }
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print(bufferIndex);
  display.println(" amostras");
  display.display();
  
  currentSessionTags = "";  // Reset tag após upload
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
  else if (cmd.equalsIgnoreCase("c")) {
    // Resetar contador de sessões
    sessionNumber = 0;
    saveSessionCount();
    Serial.println("Sessoes resetadas para 0!");
  }
  else if (cmd.equalsIgnoreCase("l")) {
    // Log de status
    Serial.println("\n=== LOG ===");
    Serial.print("Sessoes realizadas: "); Serial.println(sessionNumber);
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.println("===========\n");
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
    Serial.println("\n=== STATUS v8.0 ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.print("Buffer: "); Serial.print(BUFFER_SIZE); Serial.println(" max");
    Serial.print("Taxa: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
    Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg");
    Serial.println("===================\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS v8.0 ===");
    Serial.println("start / s    - Iniciar coleta");
    Serial.println("c            - Resetar sessoes");
    Serial.println("l            - Ver log/status");
    Serial.println("USER:nome    - Definir usuario");
    Serial.println("TAG:tag      - Definir tag da sessao");
    Serial.println("AGE:idade    - Definir idade");
    Serial.println("SEX:m/f      - Definir sexo");
    Serial.println("status       - Ver status detalhado");
    Serial.println("=====================\n");
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
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 5);
  display.println("COLETA");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Sessao #"); display.println(sessionNumber);
  display.setCursor(0, 45);
  display.println("Mantenha o dedo...");
  display.display();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - COLETA INICIADA");
  Serial.println("========================================");
  Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" segundos");
  Serial.print("Taxa alvo: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
  Serial.println("Mantenha o dedo no sensor...\n");
}

void handleCollection() {
  // ==============================================
  // LEITURA DO FIFO - CRÍTICO PARA 400Hz
  // ==============================================
  // O sensor tem um FIFO interno de 32 amostras.
  // A 400Hz, ele gera amostras a cada 2.5ms.
  // Precisamos consumir o FIFO continuamente para não perder dados.
  
  // Verifica se há amostras disponíveis no FIFO
  while (particleSensor.available()) {
    if (bufferIndex < BUFFER_SIZE) {
      // Ler do FIFO (não do "último valor")
      uint32_t irValue = particleSensor.getFIFOIR();
      uint32_t redValue = particleSensor.getFIFORed();
      
      irBuffer[bufferIndex] = (uint16_t)irValue;
      redBuffer[bufferIndex] = (uint16_t)redValue;
      bufferIndex++;
      
      // Avançar para próxima amostra no FIFO
      particleSensor.nextSample();
    } else {
      // Buffer cheio, descartar amostra
      particleSensor.nextSample();
    }
  }
  
  // Verificar se há mais dados aguardando no sensor
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
    display.setCursor(40, 5);
    display.print(secondsRemaining);
    display.println("s");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Amostras: "); display.println(bufferIndex);
    display.setCursor(0, 42);
    display.print("Taxa: "); display.print((int)currentRate); display.println(" Hz");
    display.setCursor(0, 54);
    display.print("Sessao #"); display.println(sessionNumber);
    display.display();
    
    // Log a cada 10 segundos
    if ((elapsedMS / 1000) % 10 == 0) {
      Serial.print("["); Serial.print(elapsedMS/1000); Serial.print("s] ");
      Serial.print("Amostras: "); Serial.print(bufferIndex);
      Serial.print(" | Taxa: "); Serial.print((int)currentRate); Serial.println(" Hz");
    }
  }
  
  // Verificar se terminou
  if (elapsedMS >= SAMPLE_DURATION_MS || bufferIndex >= BUFFER_SIZE) {
    float finalRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    
    Serial.println("\n========================================");
    Serial.println("   COLETA CONCLUIDA!");
    Serial.print("   Amostras: "); Serial.println(bufferIndex);
    Serial.print("   Tempo: "); Serial.print(elapsedMS/1000); Serial.println(" seg");
    Serial.print("   Taxa Real: "); Serial.print(finalRate, 1); Serial.println(" Hz");
    Serial.println("========================================\n");
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 5);
    display.println("PRONTO!");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Amostras: "); display.println(bufferIndex);
    display.setCursor(0, 42);
    display.print("Taxa: "); display.print(finalRate, 1); display.println(" Hz");
    display.setCursor(0, 54);
    display.println("Enviando dados...");
    display.display();
    
    delay(1000);
    currentState = UPLOADING;
  }
}

void handleUploading() {
  uploadRawData();
  
  delay(3000);
  currentState = WAITING_BUTTON;
  
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("Pronto para nova");
  display.setCursor(0, 32);
  display.println("medicao!");
  display.setCursor(0, 50);
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
  display.println("PulseAnalytics v8.0");
  display.setCursor(0, 12);
  display.println("MAX30102 Red+IR");
  display.setCursor(0, 28);
  display.print("User: ");
  display.println(currentUserName);
  display.setCursor(0, 40);
  display.print("Sessoes: ");
  display.println(sessionNumber);
  display.setCursor(0, 54);
  display.println("Serial: 'start'");
  display.display();
}

/**
 * ============================================
 * PULSE ANALYTICS v10.0 - STABLE INDICATOR
 * ============================================
 * 
 * BASEADO EM: v8.0 (400Hz Backup)
 * ADJUSTE: IR Brightness reduzido (0x70) para evitar clipping
 * 
 * SENSOR: MAX30102 (Red + IR apenas, sem Green)
 * FOCO: Coleta de alta frequência para análise offline
 * 
 * Configuração:
 * - LED Mode: 2 (Red + IR)
 * - Sample Rate: 800Hz
 * - Pulse Width: 215μs (Reduced for speed)
 * - IR Power: 0x70 (112) - Safety Margin
 * - Red Power: 0x7F (127)
 * 
 * Estratégia:
 * - Buffer local para 50 segundos (RAM Limitada em 800Hz)
 * - Upload único no final
 */

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
  Serial.println("   PULSE ANALYTICS v12.0 - RECOVERY 800Hz");
  Serial.println("   Sensor: MAX30102 (Red + IR) @ 800Hz");
  Serial.println("   Based on: v10 Stable");
  Serial.println("============================================");
  Serial.println("Comandos: 'start', 'USER:nome', 'help'");
  Serial.println("============================================\n");
  
  // I2C 400kHz - CRÍTICO para 400Hz de amostragem
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
  display.println("PulseAnalytics v12");
  display.setCursor(0, 25);
  display.println("RECOVERY 800Hz");
  display.setCursor(0, 40);
  display.println("Inicializando...");
  display.display();
  delay(1000);
  
  // MAX30102/MAX30105 - CONFIG PARA 400Hz
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 erro!");
    display.setCursor(0, 55);
    display.println("Sensor ERRO!");
    display.display();
    while (1);
  }
  
  // ============================================
  // CONFIGURAÇÃO PARA QUALIDADE DE SINAL PPG
  // ============================================
  // PROBLEMA ANTERIOR: pulseWidth=69 e adcRange=4096 davam sinal fraco
  // CORREÇÃO: Usar valores da v6 que funcionavam bem
  //
  // NOTA: pulseWidth=411 limita sampleRate máximo a ~400Hz
  // Isso é perfeito para nosso caso!
  
  byte ledBrightness = 0x3F;  // 63
  byte sampleAverage = 1;     // Sem média
  byte ledMode = 2;           // Red + IR (MAX30102)
  int sampleRate = 800;       // 800Hz ALVO
  int pulseWidth = 215;       // 215us (Limite para 800Hz)
  int adcRange = 16384;       // 16-bit range
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // Ajustar amplitude dos LEDs
  // CORRECAO VITAL: Em 800Hz (215us), o sensor coleta METADE da luz que em 400Hz (411us).
  // Precisamos do ganho MÁXIMO (0x7F) para compensar. 0x70 fica muito escuro/ruidoso.
  particleSensor.setPulseAmplitudeRed(0x7F);   
  particleSensor.setPulseAmplitudeIR(0x7F);    // NECESSARIO 0x7F PARA 800Hz!
  
  particleSensor.clearFIFO();
  
  Serial.println("Sensor configurado:");
  Serial.print("  LED Mode: "); Serial.println(ledMode);
  Serial.print("  Sample Rate: "); Serial.println(sampleRate);
  Serial.print("  Pulse Width: "); Serial.println(pulseWidth);
  Serial.print("  Sample Average: "); Serial.println(sampleAverage);
  Serial.println("\nSistema pronto!");
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
