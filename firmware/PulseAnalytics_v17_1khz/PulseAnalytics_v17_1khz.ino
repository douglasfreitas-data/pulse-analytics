/**
* ============================================
* PULSE ANALYTICS v17 - 1kHz PSRAM
* ============================================
* 
* BASEADO NA v15.2 RELIABLE
* 
* NOVIDADES v17:
* - PSRAM: Buffers alocados na PSRAM (8MB disponível)
* - 1000 Hz: Taxa de amostragem máxima do sensor
* - pulseWidth = 118µs (compatível com 1000Hz)
* - Duração 50s inicial para validação de qualidade
* 
* OBJETIVO:
* Validar se dados a 1000Hz são melhores que 757Hz
* antes de aumentar para coletas de 5 minutos.
* 
* SENSOR: MAX30102 (Red + IR)
* TAXA: 1000 Hz (target)
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
// OLED Display - Configuração para tela cortada
// ============================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define MARGIN_LEFT 18
#define MARGIN_RIGHT 18
#define USABLE_WIDTH (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// MAX30102 Sensor
// ============================================
MAX30105 particleSensor;

// ============================================
// CONFIGURAÇÕES - PSRAM BUFFERS
// ============================================
const int BUFFER_SIZE = 60000;  // 60 segundos @ 1000Hz
uint16_t* irBuffer = nullptr;   // Será alocado na PSRAM
uint16_t* redBuffer = nullptr;  // Será alocado na PSRAM
int bufferIndex = 0;

// Timing - 1000 Hz
const int SAMPLE_RATE_HZ = 1000;
const unsigned long SAMPLE_INTERVAL_US = 1000;  // 1000µs = 1ms = 1000Hz
const unsigned long SAMPLE_DURATION_MS = 50000; // 50 segundos para validação

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

// Taxa real medida
float realSampleRate = 0;

// Contador de retries para backoff
int retryCount = 0;

// Flag PSRAM
bool psramAvailable = false;

// ============================================
// FUNÇÕES HELPER PARA DISPLAY CENTRALIZADO
// ============================================
int getCenteredX(const char* text, int textSize) {
  int charWidth = 6 * textSize;
  int textWidth = strlen(text) * charWidth;
  return MARGIN_LEFT + (USABLE_WIDTH - textWidth) / 2;
}

int getCenteredX(String text, int textSize) {
  return getCenteredX(text.c_str(), textSize);
}

void printCentered(const char* text, int y, int textSize = 1) {
  display.setTextSize(textSize);
  display.setCursor(getCenteredX(text, textSize), y);
  display.print(text);
}

void printCentered(String text, int y, int textSize = 1) {
  printCentered(text.c_str(), y, textSize);
}

// ============================================
// PSRAM ALLOCATION
// ============================================
bool initPSRAM() {
  if (!psramFound()) {
    Serial.println("ERRO: PSRAM não encontrada!");
    return false;
  }
  
  Serial.println("PSRAM encontrada!");
  Serial.print("PSRAM Total: ");
  Serial.print(ESP.getPsramSize() / 1024);
  Serial.println(" KB");
  Serial.print("PSRAM Livre: ");
  Serial.print(ESP.getFreePsram() / 1024);
  Serial.println(" KB");
  
  // Calcular memória necessária
  size_t requiredMemory = BUFFER_SIZE * sizeof(uint16_t) * 2;  // 2 buffers
  Serial.print("Memoria necessaria: ");
  Serial.print(requiredMemory / 1024);
  Serial.println(" KB");
  
  // Alocar buffers na PSRAM
  irBuffer = (uint16_t*)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
  redBuffer = (uint16_t*)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
  
  if (!irBuffer || !redBuffer) {
    Serial.println("ERRO: Falha ao alocar buffers na PSRAM!");
    return false;
  }
  
  Serial.println("Buffers alocados na PSRAM com sucesso!");
  Serial.print("PSRAM Livre apos alocacao: ");
  Serial.print(ESP.getFreePsram() / 1024);
  Serial.println(" KB");
  
  // Inicializar buffers com zero
  memset(irBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
  memset(redBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
  
  return true;
}

// ============================================
// WiFi - COM GESTÃO MELHORADA
// ============================================

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Desconectando WiFi...");
    WiFi.disconnect(true);  // true = apagar credenciais da memória
    delay(100);
  }
}

void forceReconnectWiFi() {
  // Sempre desconectar primeiro para limpar estado
  disconnectWiFi();
  delay(500);  // Dar tempo para o roteador liberar o socket
  
  Serial.print("Conectando WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
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

// ============================================
// UPLOAD - COM CLEANUP GARANTIDO
// ============================================
void uploadRawData() {
  Serial.print("\n=== UPLOAD ATTEMPT ");
  Serial.print(retryCount + 1);
  Serial.println(" ===");
  Serial.print("Heap antes: ");
  Serial.println(ESP.getFreeHeap());
  
  // SEMPRE reconectar WiFi antes de upload
  forceReconnectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi - tentando novamente...");
    currentState = UPLOAD_FAIL;
    return;
  }
  
  // Criar cliente em escopo local para garantir destruição
  NetworkClientSecure client;
  client.setInsecure();
  client.setTimeout(60000);
  
  const char* host = "pthfxmypcxqjfstqwokf.supabase.co";
  
  display.clearDisplay();
  printCentered("Enviando...", 10);
  display.setTextSize(1);
  display.setCursor(MARGIN_LEFT, 25);
  display.print(bufferIndex);
  display.print(" amostras");
  printCentered("Aguarde...", 40);
  display.display();
  
  Serial.println("Conectando Supabase...");
  
  if (!client.connect(host, 443)) {
    Serial.print("Erro HTTPS! Heap: ");
    Serial.println(ESP.getFreeHeap());
    display.clearDisplay();
    printCentered("ERRO HTTPS!", 25, 1);
    printCentered("BTN=Retry", 45);
    display.display();
    
    // IMPORTANTE: Limpar conexão e WiFi
    client.stop();
    disconnectWiFi();
    
    currentState = UPLOAD_FAIL;
    return;
  }
  
  Serial.println("Conectado! Enviando headers...");
  
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
  sendChunk(client, "\"device_id\": \"ESP32-S3-v17-1kHz-PSRAM\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  sendChunk(client, "\"sampling_rate_hz\": " + String((int)realSampleRate) + ",");
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
  bool uploadError = false;
  
  for (int i = 0; i < bufferIndex && !uploadError; i++) {
    irBuf += String(irBuffer[i]);
    if (i < bufferIndex - 1) irBuf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, irBuf);
      irBuf = "";
      yield();
      
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload IR!");
        uploadError = true;
        break;
      }
      
      if ((i + 1) % 2000 == 0) {
        int pct = (i * 50) / bufferIndex;
        Serial.print("IR: "); Serial.print(pct); Serial.println("%");
        display.clearDisplay();
        printCentered("Enviando IR", 20);
        String pctStr = String(pct) + "%";
        printCentered(pctStr, 35);
        display.display();
      }
    }
  }
  
  if (uploadError) {
    client.stop();
    disconnectWiFi();
    currentState = UPLOAD_FAIL;
    return;
  }
  
  sendChunk(client, "],");
  Serial.println("IR OK!");
  
  // RED WAVEFORM
  Serial.println("Enviando Red waveform...");
  sendChunk(client, "\"red_waveform\": [");
  String redBuf = "";
  
  for (int i = 0; i < bufferIndex && !uploadError; i++) {
    redBuf += String(redBuffer[i]);
    if (i < bufferIndex - 1) redBuf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, redBuf);
      redBuf = "";
      yield();
      
      if (!client.connected()) {
        Serial.println("ERRO: Conexão perdida durante upload RED!");
        uploadError = true;
        break;
      }
      
      if ((i + 1) % 2000 == 0) {
        int pct = 50 + (i * 50) / bufferIndex;
        Serial.print("Red: "); Serial.print(pct); Serial.println("%");
        display.clearDisplay();
        printCentered("Enviando Red", 20);
        String pctStr = String(pct) + "%";
        printCentered(pctStr, 35);
        display.display();
      }
    }
  }
  
  if (uploadError) {
    client.stop();
    disconnectWiFi();
    currentState = UPLOAD_FAIL;
    return;
  }
  
  sendChunk(client, "]");
  Serial.println("Red OK!");
  sendChunk(client, "}");
  
  // End chunks
  client.print("0\r\n\r\n");
  
  // Aguardar resposta
  Serial.println("Aguardando resposta...");
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
  
  // SEMPRE fechar conexão e WiFi
  Serial.println("Fechando conexão...");
  client.stop();
  delay(100);
  disconnectWiFi();
  
  Serial.print("Heap depois: ");
  Serial.println(ESP.getFreeHeap());
  
  // Resultado
  display.clearDisplay();
  display.setTextSize(2);
  if (success) {
    printCentered("SUCESSO!", 15, 2);
    retryCount = 0;  // Reset contador
  } else {
    printCentered("ERRO!", 15, 2);
  }
  display.setTextSize(1);
  String samplesStr = String(bufferIndex) + " amostras";
  printCentered(samplesStr, 45);
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
      retryCount++;
      int backoffDelay = min(retryCount * 2000, 10000);  // Max 10s
      Serial.print("Retry #"); Serial.print(retryCount);
      Serial.print(" em "); Serial.print(backoffDelay/1000); Serial.println("s...");
      
      display.clearDisplay();
      printCentered("Aguardando", 15);
      String delayStr = String(backoffDelay/1000) + "s...";
      printCentered(delayStr, 35);
      display.display();
      
      delay(backoffDelay);
      currentState = UPLOADING;
    } else {
      Serial.println("Nada para reenviar.");
    }
  }
  else if (cmd.equalsIgnoreCase("l")) {
    Serial.println("\n=== LOG ===");
    Serial.print("Sessoes realizadas: "); Serial.println(sessionNumber);
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Heap livre: "); Serial.println(ESP.getFreeHeap());
    Serial.print("PSRAM livre: "); Serial.println(ESP.getFreePsram());
    Serial.print("WiFi status: "); Serial.println(WiFi.status());
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
    Serial.println("\n=== STATUS v17 1kHz ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.print("Buffer: "); Serial.print(BUFFER_SIZE); Serial.println(" max");
    Serial.print("Taxa alvo: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
    Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg");
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.print("PSRAM livre: "); Serial.println(ESP.getFreePsram());
    Serial.print("PSRAM total: "); Serial.println(ESP.getPsramSize());
    Serial.print("WiFi: "); Serial.println(WiFi.status() == WL_CONNECTED ? "CONECTADO" : "DESCONECTADO");
    Serial.println("=======================\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS v17 1kHz ===");
    Serial.println("start / s    - Iniciar coleta");
    Serial.println("c            - Resetar sessoes");
    Serial.println("l            - Ver log/status");
    Serial.println("retry / r    - Reenviar dados");
    Serial.println("USER:nome    - Definir usuario");
    Serial.println("TAG:tag      - Definir tag");
    Serial.println("AGE:idade    - Definir idade");
    Serial.println("SEX:m/f      - Definir sexo");
    Serial.println("status       - Ver status");
    Serial.println("=========================\n");
  }
}

// ============================================
// COLETA DE DADOS
// ============================================
void startCollection() {
  if (!psramAvailable) {
    Serial.println("ERRO: PSRAM não disponível!");
    display.clearDisplay();
    printCentered("ERRO PSRAM!", 25, 1);
    display.display();
    return;
  }
  
  sessionNumber++;
  saveSessionCount();
  
  bufferIndex = 0;
  particleSensor.clearFIFO();
  
  currentState = COLLECTING;
  sampleStartTime = millis();
  
  display.clearDisplay();
  printCentered("COLETA", 5, 2);
  display.setTextSize(1);
  String sessaoStr = "Sessao #" + String(sessionNumber);
  printCentered(sessaoStr, 30);
  printCentered("1kHz PSRAM", 45);
  display.display();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - COLETA 1kHz PSRAM");
  Serial.println("========================================");
  Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" segundos");
  Serial.print("Taxa alvo: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
  Serial.print("Buffer max: "); Serial.println(BUFFER_SIZE);
  Serial.println("Mantenha o dedo no sensor...\n");
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
    
    String timeStr = String(secondsRemaining) + "s";
    printCentered(timeStr, 5, 2);
    
    display.setTextSize(1);
    String amostrasStr = String(bufferIndex) + " amostras";
    printCentered(amostrasStr, 30);
    
    String taxaStr = String((int)currentRate) + " Hz";
    printCentered(taxaStr, 42);
    
    printCentered("v17 1kHz", 54);
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
    realSampleRate = finalRate;
    
    Serial.println("\n========================================");
    Serial.println("   COLETA CONCLUIDA!");
    Serial.print("   Amostras: "); Serial.println(bufferIndex);
    Serial.print("   Tempo: "); Serial.print(elapsedMS/1000); Serial.println(" seg");
    Serial.print("   Taxa Real: "); Serial.print(finalRate, 1); Serial.println(" Hz");
    Serial.println("========================================\n");
    
    display.clearDisplay();
    printCentered("PRONTO!", 5, 2);
    display.setTextSize(1);
    String amostrasStr = String(bufferIndex) + " amostras";
    printCentered(amostrasStr, 30);
    String taxaStr = String(finalRate, 1) + " Hz";
    printCentered(taxaStr, 42);
    printCentered("Enviando...", 54);
    display.display();
    
    delay(1000);
    retryCount = 0;  // Reset contador de retry
    currentState = UPLOADING;
  }
}

void handleUploading() {
  uploadRawData();
  
  if (currentState == WAITING_BUTTON) {
      delay(3000);
      display.clearDisplay();
      printCentered("Pronto!", 20);
      printCentered("Nova medicao", 32);
      printCentered("BTN ou 'start'", 50);
      display.display();
  } else if (currentState == UPLOAD_FAIL) {
      Serial.println("Falha no Upload. Pressione BOOT ou digite 'retry'.");
      display.clearDisplay();
      printCentered("FALHA!", 10, 2);
      display.setTextSize(1);
      printCentered("BTN = Retry", 35);
      printCentered("ou 'retry'", 47);
      display.display();
  }
}

void showWaitingScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  printCentered("PulseAnalytics", 0);
  printCentered("v17 1kHz PSRAM", 12);
  
  String userStr = currentUserName;
  if (userStr.length() > 12) userStr = userStr.substring(0, 12);
  printCentered(userStr, 28);
  
  String sessaoStr = "Sessoes: " + String(sessionNumber);
  printCentered(sessaoStr, 40);
  
  printCentered("BTN ou 'start'", 54);
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
  WiFi.disconnect(true);  // Limpar qualquer conexão anterior
  
  Serial.println("\n============================================");
  Serial.println("   PULSE ANALYTICS v17 - 1kHz PSRAM");
  Serial.println("   Buffers na PSRAM, 1000Hz sampling");
  Serial.println("   BOOT button = Start/Retry");
  Serial.println("============================================");
  Serial.println("Comandos: 'start', 'retry', 'help'");
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
  display.setTextColor(SSD1306_WHITE);
  printCentered("PulseAnalytics", 10);
  printCentered("v17 1kHz", 25);
  printCentered("Iniciando...", 45);
  display.display();
  delay(1000);
  
  // PSRAM - CRÍTICO!
  psramAvailable = initPSRAM();
  if (!psramAvailable) {
    display.clearDisplay();
    printCentered("ERRO!", 10, 2);
    printCentered("PSRAM nao", 35);
    printCentered("encontrada!", 47);
    display.display();
    Serial.println("ERRO FATAL: PSRAM não encontrada!");
    while (1) delay(1000);  // Parar aqui
  }
  
  // MAX30102/MAX30105
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 erro!");
    display.clearDisplay();
    printCentered("Sensor ERRO!", 25);
    display.display();
    while (1);
  }
  
  Serial.println("Executando softReset()...");
  particleSensor.softReset();
  delay(500);
  Serial.println("Sensor resetado!");
  
  // Config para 1000 Hz
  // IMPORTANTE: Para 1000Hz, pulseWidth deve ser <= 118µs
  byte ledBrightness = 0x90;  // Mantém o mesmo
  byte sampleAverage = 1;     // Sem averaging
  byte ledMode = 2;           // Red + IR
  int sampleRate = 1000;      // 1000 Hz
  int pulseWidth = 118;       // 118µs (máximo para 1000Hz) - 16 bits ADC
  int adcRange = 16384;       // Mantém
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x7F);
  particleSensor.setPulseAmplitudeIR(0x70);
  particleSensor.clearFIFO();
  
  Serial.println("Sensor configurado para 1000Hz:");
  Serial.print("  LED Mode: "); Serial.println(ledMode);
  Serial.print("  Sample Rate: "); Serial.println(sampleRate);
  Serial.print("  Pulse Width: "); Serial.print(pulseWidth); Serial.println("µs (16-bit ADC)");
  Serial.print("  ADC Range: "); Serial.println(adcRange);
  Serial.print("  Heap inicial: "); Serial.println(ESP.getFreeHeap());
  Serial.print("  PSRAM livre: "); Serial.println(ESP.getFreePsram());
  Serial.println("\nSistema pronto!");
  Serial.println("Pressione BOOT ou digite 'start'.\n");
  
  // Botão BOOT
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
  
  // Botão BOOT (GPIO 0) - FUNCIONA COMO START OU RETRY
  if (digitalRead(0) == LOW) {
    delay(50); // Debounce
    if (digitalRead(0) == LOW) {
      
      // Se aguardando: INICIAR
      if (currentState == WAITING_BUTTON) {
        Serial.println("Botao BOOT: Iniciando coleta 1kHz!");
        display.clearDisplay();
        printCentered("INICIANDO", 20, 2);
        display.display();
        delay(1000);
        startCollection();
      }
      // Se falhou upload: RETRY com backoff
      else if (currentState == UPLOAD_FAIL) {
        retryCount++;
        int backoffDelay = min(retryCount * 2000, 10000);
        Serial.print("Botao BOOT: Retry #"); Serial.print(retryCount);
        Serial.print(" em "); Serial.print(backoffDelay/1000); Serial.println("s...");
        
        display.clearDisplay();
        printCentered("RETRY...", 15, 2);
        String delayStr = String(backoffDelay/1000) + "s";
        printCentered(delayStr, 40);
        display.display();
        
        delay(backoffDelay);
        currentState = UPLOADING;
      }
      
      // Esperar soltar
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
      // Fica parado esperando BOOT ou comando 'retry'
      break;
  }
}
