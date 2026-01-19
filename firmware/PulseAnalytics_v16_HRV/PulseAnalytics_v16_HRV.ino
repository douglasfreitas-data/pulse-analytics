/**
 * ============================================
 * PULSE ANALYTICS v16.0 - HRV "MEU DIA"
 * ============================================
 * 
 * MODO: Medição Diária (HRV)
 * Coleta otimizada para análise de variabilidade cardíaca
 * 
 * SENSOR: MAX30102 (Red + IR)
 * TAXA: 125 Hz (1000Hz/8 averaging - máxima qualidade)
 * DURAÇÃO: 5 minutos (300 segundos)
 * 
 * DIFERENÇAS DA v15 (Biomarcadores):
 * - Taxa reduzida: 125 Hz (vs 800 Hz) - coincide com MIMIC-II
 * - Duração maior: 5 min (vs 50s)
 * - Foco: Intervalos RR, não morfologia
 * - Buffer: 40.000 amostras (5 min @ 125Hz = 37.500)
 * - sampleAverage=8: máximo SNR (média de 8 leituras)
 * 
 * DECISÃO TÉCNICA:
 * 125 Hz escolhido para compatibilidade direta com MIMIC-II.
 * 8x averaging no hardware = sinal muito limpo.
 * (Ver: docs/estrutura/V2 - DECISÃO DE FREQUÊNCIA.md)
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
// CONFIGURAÇÕES v16.0 - HRV MODE
// ============================================

// Buffer para 200 segundos @ 200Hz = 40.000 amostras
// RAM: 40k * 2ch * 2 bytes = 160KB (seguro para ESP32-S3)
// Nota: Para 5 minutos completos, seria necessário streaming ou só IR
const int BUFFER_SIZE = 40000;
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Timing - MODO HRV (125 Hz = 5 min = 37.500 amostras)
const int SAMPLE_RATE_HZ = 125;           // Taxa efetiva (1000/8)
const unsigned long SAMPLE_INTERVAL_US = 8000;  // 1000000 / 125 = 8000μs
const unsigned long SAMPLE_DURATION_MS = 300000; // 5 minutos = 300 segundos

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
  UPLOADING,
  UPLOAD_FAIL
};
DeviceState currentState = WAITING_BUTTON;

// Taxa real medida (para upload)
float realSampleRate = 0;

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
  display.println("Aguarde (5min dados)");
  display.display();
  
  Serial.println(ESP.getFreeHeap());
  
  // TIMEOUT ESTENDIDO para upload de 5 minutos de dados
  client.setTimeout(120000);  // 2 minutos de timeout

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
  sendChunk(client, "\"device_id\": \"ESP32-S3-v16-HRV\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  sendChunk(client, "\"sampling_rate_hz\": " + String((int)realSampleRate) + ",");
  sendChunk(client, "\"session_index\": " + String(sessionNumber) + ",");
  sendChunk(client, "\"timestamp_device_min\": " + String((millis() - bootTime) / 60000) + ",");
  
  // Métricas null (serão calculadas offline)
  sendChunk(client, "\"fc_mean\": null,");
  sendChunk(client, "\"sdnn\": null,");
  sendChunk(client, "\"rmssd\": null,");
  sendChunk(client, "\"pnn50\": null,");
  sendChunk(client, "\"rr_valid_count\": null,");
  sendChunk(client, "\"rrr_intervals_ms\": null,");
  sendChunk(client, "\"green_waveform\": null,");
  
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
  Serial.println("Enviando IR waveform (60k amostras)...");
  sendChunk(client, "\"ir_waveform\": [");
  String irBuf = "";
  for (int i = 0; i < bufferIndex; i++) {
    irBuf += String(irBuffer[i]);
    if (i < bufferIndex - 1) irBuf += ",";
    
    // Flush buffer a cada 200 valores
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, irBuf);
      irBuf = "";
      
      yield();
      
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload IR!");
        currentState = UPLOAD_FAIL;
        return;
      }
      
      // Progress a cada 5000 valores
      if ((i + 1) % 5000 == 0) {
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
  Serial.println("Enviando Red waveform (60k amostras)...");
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
      
      if ((i + 1) % 5000 == 0) {
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
  while (client.connected() && millis() - timeout < 60000) {
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
  
  currentSessionTags = "";
  
  if (success) {
    currentState = WAITING_BUTTON;
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
    sessionNumber = 0;
    saveSessionCount();
    Serial.println("Sessoes resetadas para 0!");
  }
  else if (cmd.equalsIgnoreCase("retry") || cmd.equalsIgnoreCase("r")) {
    if (currentState == UPLOAD_FAIL) {
      Serial.println("Retentando upload...");
      currentState = UPLOADING;
    } else {
      Serial.println("Nada para reenviar.");
    }
  }
  else if (cmd.equalsIgnoreCase("l")) {
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
    Serial.println("\n=== STATUS v16.0 HRV ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.print("Buffer: "); Serial.print(BUFFER_SIZE); Serial.println(" max");
    Serial.print("Taxa: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
    Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg (5 min)");
    Serial.println("========================\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS v16.0 HRV ===");
    Serial.println("start / s    - Iniciar coleta (5 min)");
    Serial.println("c            - Resetar sessoes");
    Serial.println("l            - Ver log/status");
    Serial.println("USER:nome    - Definir usuario");
    Serial.println("TAG:tag      - Definir tag da sessao");
    Serial.println("AGE:idade    - Definir idade");
    Serial.println("SEX:m/f      - Definir sexo");
    Serial.println("status       - Ver status detalhado");
    Serial.println("==========================\n");
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
  display.println("MEU DIA");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Sessao #"); display.println(sessionNumber);
  display.setCursor(0, 45);
  display.println("Mantenha o dedo...");
  display.display();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - MEU DIA (HRV)");
  Serial.println("========================================");
  Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" segundos (5 min)");
  Serial.print("Taxa: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
  Serial.println("Mantenha o dedo no sensor...\n");
}

void handleCollection() {
  // Leitura do FIFO - 200Hz é bem mais tranquilo que 800Hz
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
    int minutesRemaining = secondsRemaining / 60;
    int secsRemaining = secondsRemaining % 60;
    float currentRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(30, 5);
    // Mostrar MM:SS
    if (minutesRemaining < 10) display.print("0");
    display.print(minutesRemaining);
    display.print(":");
    if (secsRemaining < 10) display.print("0");
    display.println(secsRemaining);
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Amostras: "); display.println(bufferIndex);
    display.setCursor(0, 42);
    display.print("Taxa: "); display.print((int)currentRate); display.println(" Hz");
    display.setCursor(0, 54);
    display.print("MEU DIA #"); display.println(sessionNumber);
    display.display();
    
    // Log a cada 30 segundos
    if ((elapsedMS / 1000) % 30 == 0) {
      Serial.print("["); Serial.print(elapsedMS/1000); Serial.print("s] ");
      Serial.print("Amostras: "); Serial.print(bufferIndex);
      Serial.print(" | Taxa: "); Serial.print((int)currentRate); Serial.println(" Hz");
    }
  }
  
  // Verificar se terminou
  if (elapsedMS >= SAMPLE_DURATION_MS || bufferIndex >= BUFFER_SIZE) {
    float finalRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    realSampleRate = finalRate;
    
    Serial.println("\n========================================");
    Serial.println("   MEU DIA - COLETA CONCLUIDA!");
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
  
  if (currentState == WAITING_BUTTON) {
      delay(3000);
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println("Pronto para nova");
      display.setCursor(0, 32);
      display.println("medicao!");
      display.setCursor(0, 50);
      display.println("Digite 'start'");
      display.display();
  } else if (currentState == UPLOAD_FAIL) {
      Serial.println("Falha no Upload. Digite 'retry' para tentar novamente.");
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10);
      display.println("FALHA!");
      display.setTextSize(1);
      display.setCursor(0, 35);
      display.println("Digite 'retry'");
      display.println("para reenviar.");
      display.display();
  }
}

void showWaitingScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("PulseAnalytics v16.0");
  display.setCursor(0, 12);
  display.println("MEU DIA (HRV 200Hz)");
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
  Serial.println("   PULSE ANALYTICS v16.0 - MEU DIA (HRV)");
  Serial.println("   Sensor: MAX30102 (Red + IR) @ 200Hz");
  Serial.println("   Duracao: 5 minutos");
  Serial.println("   Modo: Medição Diária (HRV)");
  Serial.println("============================================");
  Serial.println("Comandos: 'start', 'USER:nome', 'help'");
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
  display.println("PulseAnalytics v16");
  display.setCursor(0, 25);
  display.println("MEU DIA - HRV 200Hz");
  display.setCursor(0, 40);
  display.println("Inicializando...");
  display.display();
  delay(1000);
  
  // MAX30102 - CONFIG PARA 200Hz (HRV otimizado)
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 erro!");
    display.setCursor(0, 55);
    display.println("Sensor ERRO!");
    display.display();
    while (1);
  }
  
  Serial.println("Executando softReset()...");
  particleSensor.softReset();
  delay(500);
  Serial.println("Sensor resetado!");
  
  // ============================================
  // CONFIGURAÇÃO PARA HRV (125 Hz - MÁXIMA QUALIDADE)
  // ============================================
  // 1000 Hz com 8x averaging = 125 Hz efetivo (igual MIMIC-II)
  // pulseWidth=118 permite 1000Hz, LED mais alto compensa
  // 8x averaging = sinal muito limpo, ideal para detecção de picos
  
  byte ledBrightness = 0x80;  // Aumentado para compensar PW menor
  byte sampleAverage = 8;     // 8x averaging = máximo SNR
  byte ledMode = 2;           // Red + IR
  int sampleRate = 1000;      // 1000 Hz / 8 = 125 Hz efetivo
  int pulseWidth = 118;       // Menor PW para permitir 1000Hz
  int adcRange = 16384;       // 16-bit
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // Amplitudes mais altas para compensar pulseWidth menor
  particleSensor.setPulseAmplitudeRed(0x80);   
  particleSensor.setPulseAmplitudeIR(0x80);
  
  particleSensor.clearFIFO();
  
  Serial.println("Sensor configurado (HRV Mode - 125Hz):");
  Serial.print("  LED Mode: "); Serial.println(ledMode);
  Serial.print("  Sample Rate Config: "); Serial.println(sampleRate);
  Serial.print("  Sample Average: "); Serial.println(sampleAverage);
  Serial.print("  Taxa Efetiva: "); Serial.print(sampleRate / sampleAverage); Serial.println(" Hz");
  Serial.print("  Pulse Width: "); Serial.println(pulseWidth);
  Serial.println("\nSistema pronto!");
  Serial.println("Digite 'start' ou pressione BOOT para iniciar MEU DIA (5 min).\n");
  
  // Botão BOOT (GPIO 0) para iniciar sem Serial
  pinMode(0, INPUT_PULLUP);
  
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
  
  // Botão BOOT (GPIO 0) para iniciar manualmente
  if (digitalRead(0) == LOW) {
    delay(50); // Debounce
    if (digitalRead(0) == LOW) {
      if (currentState == WAITING_BUTTON) {
        Serial.println("Botao BOOT pressionado!");
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 20);
        display.println("INICIANDO");
        display.display();
        delay(1000);
        startCollection();
      }
      // Esperar soltar para não disparar multiplas vezes
      while (digitalRead(0) == LOW) delay(10);
    }
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
  }
}
