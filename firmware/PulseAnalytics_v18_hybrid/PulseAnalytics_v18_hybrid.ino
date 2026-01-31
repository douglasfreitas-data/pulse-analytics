/**
 * ============================================
 * PULSE ANALYTICS v18 - HYBRID (RAM + PSRAM)
 * ============================================
 * 
 * ARQUITETURA DUAL-CORE:
 * - Core 0: Coleta rápida do FIFO → Ring buffer (RAM)
 * - Core 1: Transferência paralela → PSRAM
 * 
 * OBJETIVO:
 * Combinar velocidade da RAM interna com capacidade
 * da PSRAM, eliminando perda de amostras por latência.
 * 
 * SENSOR: MAX30102 (Red + IR)
 * TAXA: ~757 Hz real (800Hz configurado)
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

#define MARGIN_LEFT 18
#define MARGIN_RIGHT 18
#define USABLE_WIDTH (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// MAX30102 Sensor
// ============================================
MAX30105 particleSensor;

// ============================================
// RING BUFFER NA RAM INTERNA (coleta rápida)
// ============================================
const int RING_SIZE = 8000;        // ~10 segundos @ 757Hz
uint16_t ringIR[RING_SIZE];        // 16KB na RAM
uint16_t ringRed[RING_SIZE];       // 16KB na RAM
volatile int ringWriteIndex = 0;
volatile int ringReadIndex = 0;

// ============================================
// BUFFERS NA PSRAM (armazenamento grande)
// ============================================
const int PSRAM_SIZE = 50000;      // ~60 segundos @ 757Hz
uint16_t* irBuffer = nullptr;
uint16_t* redBuffer = nullptr;
volatile int bufferIndex = 0;

// ============================================
// CONFIGURAÇÕES DE TIMING
// ============================================
const int SAMPLE_RATE_HZ = 800;
const unsigned long SAMPLE_DURATION_MS = 60000;  // 60 segundos

unsigned long sampleStartTime = 0;
unsigned long bootTime = 0;

// ============================================
// WiFi & Supabase
// ============================================
const char* WIFI_SSID = "Freitas";
const char* WIFI_PASS = "2512Jesus";
const char* SUPABASE_URL = "https://pthfxmypcxqjfstqwokf.supabase.co/rest/v1/hrv_sessions";
const char* SUPABASE_KEY = "sb_publishable_V4ZrfeZNld9VROJOWZcE_w_N93BHvqd";

// ============================================
// SESSÃO
// ============================================
int sessionNumber = 0;
String currentUserName = "Visitante";
String currentSessionTags = "";
int currentSessionAge = 0;
String currentSessionGender = "";

// ============================================
// ESTADOS E FLAGS
// ============================================
enum DeviceState {
  WAITING_BUTTON,
  COLLECTING,
  UPLOADING,
  UPLOAD_FAIL
};
DeviceState currentState = WAITING_BUTTON;

float realSampleRate = 0;
int retryCount = 0;
bool psramAvailable = false;

// ============================================
// FREERTOS - DUAL CORE
// ============================================
TaskHandle_t transferTaskHandle = NULL;
volatile bool collectionActive = false;
volatile int transferCount = 0;

// ============================================
// FUNÇÕES HELPER DISPLAY
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
  
  irBuffer = (uint16_t*)ps_malloc(PSRAM_SIZE * sizeof(uint16_t));
  redBuffer = (uint16_t*)ps_malloc(PSRAM_SIZE * sizeof(uint16_t));
  
  if (!irBuffer || !redBuffer) {
    Serial.println("ERRO: Falha ao alocar PSRAM!");
    return false;
  }
  
  memset(irBuffer, 0, PSRAM_SIZE * sizeof(uint16_t));
  memset(redBuffer, 0, PSRAM_SIZE * sizeof(uint16_t));
  
  Serial.println("Buffers PSRAM alocados!");
  Serial.print("PSRAM Livre: ");
  Serial.print(ESP.getFreePsram() / 1024);
  Serial.println(" KB");
  
  return true;
}

// ============================================
// TASK CORE 1 - TRANSFERÊNCIA PARA PSRAM
// ============================================
void transferTask(void* parameter) {
  Serial.println("[Core 1] Task de transferência iniciada");
  
  while (true) {
    // Aguarda 100ms e verifica se há dados para transferir
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    if (!collectionActive) continue;
    
    // Calcula quantos dados estão disponíveis no ring buffer
    int available = (ringWriteIndex - ringReadIndex + RING_SIZE) % RING_SIZE;
    
    // Transfere quando há pelo menos 1000 amostras
    if (available >= 1000) {
      int toTransfer = available;
      
      // Limita para não exceder PSRAM
      if (bufferIndex + toTransfer > PSRAM_SIZE) {
        toTransfer = PSRAM_SIZE - bufferIndex;
      }
      
      if (toTransfer > 0) {
        // Transferência em bloco
        for (int i = 0; i < toTransfer; i++) {
          irBuffer[bufferIndex] = ringIR[ringReadIndex];
          redBuffer[bufferIndex] = ringRed[ringReadIndex];
          ringReadIndex = (ringReadIndex + 1) % RING_SIZE;
          bufferIndex++;
        }
        
        transferCount++;
        
        if (transferCount % 10 == 0) {
          Serial.print("[Core 1] Transferência #");
          Serial.print(transferCount);
          Serial.print(" | PSRAM: ");
          Serial.print(bufferIndex);
          Serial.println(" amostras");
        }
      }
    }
  }
}

// ============================================
// WIFI
// ============================================
void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    delay(100);
  }
}

void forceReconnectWiFi() {
  disconnectWiFi();
  delay(500);
  
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
// UPLOAD
// ============================================
void uploadRawData() {
  Serial.print("\n=== UPLOAD v18 HYBRID ===\n");
  Serial.print("Amostras: "); Serial.println(bufferIndex);
  
  forceReconnectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    currentState = UPLOAD_FAIL;
    return;
  }
  
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
  printCentered("v18 Hybrid", 40);
  display.display();
  
  if (!client.connect(host, 443)) {
    Serial.println("Erro HTTPS!");
    client.stop();
    disconnectWiFi();
    currentState = UPLOAD_FAIL;
    return;
  }
  
  client.println("POST /rest/v1/hrv_sessions HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Content-Type: application/json");
  client.print("apikey: "); client.println(SUPABASE_KEY);
  client.print("Authorization: Bearer "); client.println(SUPABASE_KEY);
  client.println("Prefer: return=minimal");
  client.println("Transfer-Encoding: chunked");
  client.println("Connection: close");
  client.println();
  
  sendChunk(client, "{");
  sendChunk(client, "\"device_id\": \"ESP32-S3-v18-Hybrid\",");
  sendChunk(client, "\"user_name\": \"" + currentUserName + "\",");
  sendChunk(client, "\"sampling_rate_hz\": " + String((int)realSampleRate) + ",");
  sendChunk(client, "\"session_index\": " + String(sessionNumber) + ",");
  sendChunk(client, "\"timestamp_device_min\": " + String((millis() - bootTime) / 60000) + ",");
  
  sendChunk(client, "\"fc_mean\": null,");
  sendChunk(client, "\"sdnn\": null,");
  sendChunk(client, "\"rmssd\": null,");
  sendChunk(client, "\"pnn50\": null,");
  sendChunk(client, "\"rr_valid_count\": null,");
  sendChunk(client, "\"rrr_intervals_ms\": null,");
  sendChunk(client, "\"green_waveform\": null,");
  
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
  
  // IR WAVEFORM
  Serial.println("Enviando IR...");
  sendChunk(client, "\"ir_waveform\": [");
  String buf = "";
  bool uploadError = false;
  
  for (int i = 0; i < bufferIndex && !uploadError; i++) {
    buf += String(irBuffer[i]);
    if (i < bufferIndex - 1) buf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, buf);
      buf = "";
      yield();
      
      if (!client.connected()) {
        uploadError = true;
        break;
      }
      
      if ((i + 1) % 5000 == 0) {
        int pct = (i * 50) / bufferIndex;
        display.clearDisplay();
        printCentered("Enviando IR", 20);
        printCentered(String(pct) + "%", 35);
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
  
  // RED WAVEFORM
  Serial.println("Enviando Red...");
  sendChunk(client, "\"red_waveform\": [");
  buf = "";
  
  for (int i = 0; i < bufferIndex && !uploadError; i++) {
    buf += String(redBuffer[i]);
    if (i < bufferIndex - 1) buf += ",";
    
    if ((i + 1) % 200 == 0 || i == bufferIndex - 1) {
      sendChunk(client, buf);
      buf = "";
      yield();
      
      if (!client.connected()) {
        uploadError = true;
        break;
      }
      
      if ((i + 1) % 5000 == 0) {
        int pct = 50 + (i * 50) / bufferIndex;
        display.clearDisplay();
        printCentered("Enviando Red", 20);
        printCentered(String(pct) + "%", 35);
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
  sendChunk(client, "}");
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
        }
      }
      if (line == "\r") break;
      timeout = millis();
    }
  }
  
  client.stop();
  disconnectWiFi();
  
  display.clearDisplay();
  display.setTextSize(2);
  if (success) {
    printCentered("SUCESSO!", 15, 2);
    retryCount = 0;
  } else {
    printCentered("ERRO!", 15, 2);
  }
  display.setTextSize(1);
  printCentered(String(bufferIndex) + " amostras", 45);
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
  else if (cmd.equalsIgnoreCase("retry") || cmd.equalsIgnoreCase("r")) {
    if (currentState == UPLOAD_FAIL) {
      retryCount++;
      int backoff = min(retryCount * 2000, 10000);
      delay(backoff);
      currentState = UPLOADING;
    }
  }
  else if (cmd.equalsIgnoreCase("l")) {
    Serial.println("\n=== LOG v18 ===");
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.print("PSRAM: "); Serial.println(ESP.getFreePsram());
    Serial.print("Ring Write: "); Serial.println(ringWriteIndex);
    Serial.print("Ring Read: "); Serial.println(ringReadIndex);
    Serial.print("Buffer PSRAM: "); Serial.println(bufferIndex);
    Serial.println("===============\n");
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
    Serial.println("\n=== STATUS v18 HYBRID ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.print("Ring Size: "); Serial.println(RING_SIZE);
    Serial.print("PSRAM Size: "); Serial.println(PSRAM_SIZE);
    Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg");
    Serial.println("=========================\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS v18 HYBRID ===");
    Serial.println("start / s    - Iniciar coleta");
    Serial.println("c            - Resetar sessoes");
    Serial.println("l            - Ver log");
    Serial.println("retry / r    - Reenviar dados");
    Serial.println("USER:nome    - Usuario");
    Serial.println("TAG:tag      - Tag");
    Serial.println("AGE:idade    - Idade");
    Serial.println("SEX:m/f      - Sexo");
    Serial.println("status       - Status");
    Serial.println("===========================\n");
  }
}

// ============================================
// COLETA DE DADOS
// ============================================
void startCollection() {
  if (!psramAvailable) {
    Serial.println("ERRO: PSRAM não disponível!");
    return;
  }
  
  sessionNumber++;
  saveSessionCount();
  
  // Reset buffers
  ringWriteIndex = 0;
  ringReadIndex = 0;
  bufferIndex = 0;
  transferCount = 0;
  
  particleSensor.clearFIFO();
  
  collectionActive = true;
  currentState = COLLECTING;
  sampleStartTime = millis();
  
  display.clearDisplay();
  printCentered("COLETA", 5, 2);
  display.setTextSize(1);
  printCentered("Sessao #" + String(sessionNumber), 30);
  printCentered("v18 Hybrid", 45);
  display.display();
  
  Serial.println("\n========================================");
  Serial.print("   SESSAO #"); Serial.print(sessionNumber);
  Serial.println(" - v18 HYBRID");
  Serial.println("========================================");
  Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg");
  Serial.println("Core 0: Coleta rapida (RAM)");
  Serial.println("Core 1: Transferencia (PSRAM)");
  Serial.println("Mantenha o dedo no sensor...\n");
}

void handleCollection() {
  // CORE 0: Leitura rápida do FIFO → Ring buffer (RAM)
  while (particleSensor.available()) {
    int nextIndex = (ringWriteIndex + 1) % RING_SIZE;
    
    // Verifica se há espaço no ring buffer
    if (nextIndex != ringReadIndex) {
      ringIR[ringWriteIndex] = (uint16_t)particleSensor.getFIFOIR();
      ringRed[ringWriteIndex] = (uint16_t)particleSensor.getFIFORed();
      ringWriteIndex = nextIndex;
    }
    
    particleSensor.nextSample();
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
    printCentered(String(secondsRemaining) + "s", 5, 2);
    
    display.setTextSize(1);
    printCentered(String(bufferIndex) + " amostras", 30);
    printCentered(String((int)currentRate) + " Hz", 42);
    printCentered("v18 Hybrid", 54);
    display.display();
    
    if ((elapsedMS / 1000) % 10 == 0) {
      int ringUsed = (ringWriteIndex - ringReadIndex + RING_SIZE) % RING_SIZE;
      Serial.print("["); Serial.print(elapsedMS/1000); Serial.print("s] ");
      Serial.print("PSRAM: "); Serial.print(bufferIndex);
      Serial.print(" | Ring: "); Serial.print(ringUsed);
      Serial.print(" | Taxa: "); Serial.print((int)currentRate); Serial.println(" Hz");
    }
  }
  
  // Verificar término
  if (elapsedMS >= SAMPLE_DURATION_MS || bufferIndex >= PSRAM_SIZE) {
    collectionActive = false;
    
    // Aguarda última transferência
    delay(200);
    
    // Transfere dados restantes do ring buffer
    while (ringReadIndex != ringWriteIndex && bufferIndex < PSRAM_SIZE) {
      irBuffer[bufferIndex] = ringIR[ringReadIndex];
      redBuffer[bufferIndex] = ringRed[ringReadIndex];
      ringReadIndex = (ringReadIndex + 1) % RING_SIZE;
      bufferIndex++;
    }
    
    float finalRate = (elapsedMS > 0) ? (float)bufferIndex / (elapsedMS / 1000.0) : 0;
    realSampleRate = finalRate;
    
    Serial.println("\n========================================");
    Serial.println("   COLETA CONCLUIDA!");
    Serial.print("   Amostras: "); Serial.println(bufferIndex);
    Serial.print("   Taxa Real: "); Serial.print(finalRate, 1); Serial.println(" Hz");
    Serial.print("   Transferencias: "); Serial.println(transferCount);
    Serial.println("========================================\n");
    
    display.clearDisplay();
    printCentered("PRONTO!", 5, 2);
    display.setTextSize(1);
    printCentered(String(bufferIndex) + " amostras", 30);
    printCentered(String(finalRate, 1) + " Hz", 42);
    printCentered("Enviando...", 54);
    display.display();
    
    delay(1000);
    retryCount = 0;
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
    Serial.println("Falha no Upload. BTN ou 'retry'.");
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
  printCentered("v18 Hybrid", 12);
  
  String userStr = currentUserName;
  if (userStr.length() > 12) userStr = userStr.substring(0, 12);
  printCentered(userStr, 28);
  
  printCentered("Sessoes: " + String(sessionNumber), 40);
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
  WiFi.disconnect(true);
  
  Serial.println("\n============================================");
  Serial.println("   PULSE ANALYTICS v18 - HYBRID");
  Serial.println("   RAM (coleta) + PSRAM (armazenamento)");
  Serial.println("   Dual-Core: Core0=Coleta, Core1=Transfer");
  Serial.println("============================================");
  Serial.println("Comandos: 'start', 'retry', 'help'");
  Serial.println("============================================\n");
  
  Wire.begin();
  Wire.setClock(400000);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED erro!");
    while (1);
  }
  
  initSPIFFS();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  printCentered("PulseAnalytics", 10);
  printCentered("v18 Hybrid", 25);
  printCentered("Iniciando...", 45);
  display.display();
  delay(1000);
  
  // Inicializar PSRAM
  psramAvailable = initPSRAM();
  if (!psramAvailable) {
    display.clearDisplay();
    printCentered("ERRO!", 10, 2);
    printCentered("PSRAM nao", 35);
    printCentered("encontrada!", 47);
    display.display();
    Serial.println("ERRO FATAL: PSRAM não encontrada!");
    while (1) delay(1000);
  }
  
  // Inicializar sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 erro!");
    display.clearDisplay();
    printCentered("Sensor ERRO!", 25);
    display.display();
    while (1);
  }
  
  particleSensor.softReset();
  delay(500);
  
  // Configuração do sensor (mesma da v15/v17)
  byte ledBrightness = 0x90;
  byte sampleAverage = 1;
  byte ledMode = 2;
  int sampleRate = 800;
  int pulseWidth = 215;
  int adcRange = 16384;
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x7F);
  particleSensor.setPulseAmplitudeIR(0x70);
  particleSensor.clearFIFO();
  
  Serial.println("Sensor configurado: 800Hz, pulseWidth=215");
  
  // Criar Task no Core 1 para transferência
  xTaskCreatePinnedToCore(
    transferTask,          // Função
    "TransferTask",        // Nome
    4096,                  // Stack size
    NULL,                  // Parâmetro
    1,                     // Prioridade
    &transferTaskHandle,   // Handle
    1                      // Core 1
  );
  
  Serial.println("Task de transferência criada no Core 1");
  
  pinMode(0, INPUT_PULLUP);
  
  currentState = WAITING_BUTTON;
  
  Serial.println("\nSistema pronto!");
  Serial.println("Pressione BOOT ou digite 'start'.\n");
}

// ============================================
// LOOP (CORE 0)
// ============================================
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  
  // Botão BOOT
  if (digitalRead(0) == LOW) {
    delay(50);
    if (digitalRead(0) == LOW) {
      
      if (currentState == WAITING_BUTTON) {
        Serial.println("Botao BOOT: Iniciando!");
        display.clearDisplay();
        printCentered("INICIANDO", 20, 2);
        display.display();
        delay(1000);
        startCollection();
      }
      else if (currentState == UPLOAD_FAIL) {
        retryCount++;
        int backoff = min(retryCount * 2000, 10000);
        display.clearDisplay();
        printCentered("RETRY...", 15, 2);
        printCentered(String(backoff/1000) + "s", 40);
        display.display();
        delay(backoff);
        currentState = UPLOADING;
      }
      
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
      break;
  }
}
