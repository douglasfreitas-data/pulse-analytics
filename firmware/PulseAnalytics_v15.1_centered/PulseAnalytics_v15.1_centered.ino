/**
* ============================================
* PULSE ANALYTICS v15.1 - CENTERED DISPLAY
* ============================================
* 
* BASEADO NA v15.0 OPTIMAL
* 
* MELHORIAS v15.1:
* - Textos OLED centralizados (para telas cortadas)
* - Botão BOOT funciona como RETRY quando upload falha
* - Margem de 3 chars (~18px) em cada lado
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
// OLED Display - Configuração para tela cortada
// ============================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Margem para tela cortada (3 chars = ~18 pixels em textSize 1)
#define MARGIN_LEFT 18
#define MARGIN_RIGHT 18
#define USABLE_WIDTH (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// MAX30102 Sensor
// ============================================
MAX30105 particleSensor;

// ============================================
// CONFIGURAÇÕES
// ============================================
const int BUFFER_SIZE = 40000;
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Timing
const int SAMPLE_RATE_HZ = 800;
const unsigned long SAMPLE_INTERVAL_US = 1250;
const unsigned long SAMPLE_DURATION_MS = 50000;

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

// ============================================
// FUNÇÕES HELPER PARA DISPLAY CENTRALIZADO
// ============================================

// Calcula posição X para centralizar texto na área útil
int getCenteredX(const char* text, int textSize) {
  int charWidth = 6 * textSize;  // 6 pixels por char em fonte padrão
  int textWidth = strlen(text) * charWidth;
  return MARGIN_LEFT + (USABLE_WIDTH - textWidth) / 2;
}

int getCenteredX(String text, int textSize) {
  return getCenteredX(text.c_str(), textSize);
}

// Escreve texto centralizado
void printCentered(const char* text, int y, int textSize = 1) {
  display.setTextSize(textSize);
  display.setCursor(getCenteredX(text, textSize), y);
  display.print(text);
}

void printCentered(String text, int y, int textSize = 1) {
  printCentered(text.c_str(), y, textSize);
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
  printCentered("Enviando...", 10);
  display.setTextSize(1);
  display.setCursor(MARGIN_LEFT, 25);
  display.print(bufferIndex);
  display.print(" amostras");
  printCentered("Aguarde...", 40);
  display.display();
  
  Serial.println(ESP.getFreeHeap());
  
  client.setTimeout(60000); 

  if (!client.connect(host, 443)) {
    Serial.print("Erro HTTPS! Heap: ");
    Serial.println(ESP.getFreeHeap());
    display.clearDisplay();
    printCentered("ERRO HTTPS!", 25, 1);
    printCentered("BTN=Retry", 45);
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
  sendChunk(client, "\"device_id\": \"ESP32-S3-v15.1-Centered\",");
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
        display.clearDisplay();
        printCentered("Enviando IR", 20);
        String pctStr = String(pct) + "%";
        printCentered(pctStr, 35);
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
        display.clearDisplay();
        printCentered("Enviando Red", 20);
        String pctStr = String(pct) + "%";
        printCentered(pctStr, 35);
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
  if (success) {
    printCentered("SUCESSO!", 15, 2);
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
    Serial.println("\n=== STATUS v15.1 ===");
    Serial.print("Usuario: "); Serial.println(currentUserName);
    Serial.print("Sessoes: "); Serial.println(sessionNumber);
    Serial.print("Estado: "); Serial.println(currentState == WAITING_BUTTON ? "AGUARDANDO" : "COLETANDO");
    Serial.print("Buffer: "); Serial.print(BUFFER_SIZE); Serial.println(" max");
    Serial.print("Taxa: "); Serial.print(SAMPLE_RATE_HZ); Serial.println(" Hz");
    Serial.print("Duracao: "); Serial.print(SAMPLE_DURATION_MS/1000); Serial.println(" seg");
    Serial.println("===================\n");
  }
  else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("h")) {
    Serial.println("\n=== COMANDOS v15.1 ===");
    Serial.println("start / s    - Iniciar coleta");
    Serial.println("c            - Resetar sessoes");
    Serial.println("l            - Ver log/status");
    Serial.println("retry / r    - Reenviar dados");
    Serial.println("USER:nome    - Definir usuario");
    Serial.println("TAG:tag      - Definir tag");
    Serial.println("AGE:idade    - Definir idade");
    Serial.println("SEX:m/f      - Definir sexo");
    Serial.println("status       - Ver status");
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
  printCentered("COLETA", 5, 2);
  display.setTextSize(1);
  String sessaoStr = "Sessao #" + String(sessionNumber);
  printCentered(sessaoStr, 30);
  printCentered("Mantenha dedo", 45);
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
    
    // Tempo restante centralizado grande
    String timeStr = String(secondsRemaining) + "s";
    printCentered(timeStr, 5, 2);
    
    display.setTextSize(1);
    String amostrasStr = String(bufferIndex) + " amostras";
    printCentered(amostrasStr, 30);
    
    String taxaStr = String((int)currentRate) + " Hz";
    printCentered(taxaStr, 42);
    
    String sessaoStr = "Sessao #" + String(sessionNumber);
    printCentered(sessaoStr, 54);
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
  printCentered("v15.1 Centered", 12);
  
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
  WiFi.disconnect();
  
  Serial.println("\n============================================");
  Serial.println("   PULSE ANALYTICS v15.1 - CENTERED");
  Serial.println("   Display otimizado para tela cortada");
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
  printCentered("v15.1", 25);
  printCentered("Iniciando...", 45);
  display.display();
  delay(1000);
  
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
  
  // Config R08 do Teste Matrix
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
  
  Serial.println("Sensor configurado:");
  Serial.print("  LED Mode: "); Serial.println(ledMode);
  Serial.print("  Sample Rate: "); Serial.println(sampleRate);
  Serial.print("  Pulse Width: "); Serial.println(pulseWidth);
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
        Serial.println("Botao BOOT: Iniciando coleta!");
        display.clearDisplay();
        printCentered("INICIANDO", 20, 2);
        display.display();
        delay(1000);
        startCollection();
      }
      // Se falhou upload: RETRY
      else if (currentState == UPLOAD_FAIL) {
        Serial.println("Botao BOOT: Retry upload!");
        display.clearDisplay();
        printCentered("RETRY...", 20, 2);
        display.display();
        delay(500);
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
