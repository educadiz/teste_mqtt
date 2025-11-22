/* 
  Código ajustado para teste de dashboard no Thinkspeak.com
  A conexão para dados junto ao thinkspeak foi feita via API.
  Os tópicos de MQTT tradicional estão apontando para o broker
  definido na variavel mqtt.server.
  Observe os dados relativos à conexão após linha de comentários
  do thinkspeak.
  
  ATUALIZAÇÃO: Código ajustado para GMT020-02-7P v1.3 com Adafruit_ST7789
  
  BIBLIOTECAS NECESSÁRIAS:
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library
  - DHT sensor library
  - PubSubClient
  - Blynk
  
  Instale via Library Manager do Arduino IDE.
*/

// Configuração Blynk: Ajuste suas credenciais abaixo:
#define BLYNK_TEMPLATE_ID "TMPL2EzjqzR7P"
#define BLYNK_TEMPLATE_NAME "EMC01"
#define BLYNK_AUTH_TOKEN "y0YIz5He8zrIW1BF-0eHwQAN7VdxJkAK"
#define BLYNK_PRINT Serial

// Bibliotecas:
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <DHT.h>
#include <time.h>
#include <BlynkSimpleEsp32.h>

// Hardware: Pinos de sensores e atuadores
#define DHTPIN 32
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define LED_VERDE 16
#define LED_VERMELHO 17
#define BUZZER 2
#define CHUVA_PIN 34
#define SOLAR_PIN 33  

// Display GMT020-02-7P v1.3 (TFT 2" ST7789):
#define SCREEN_WIDTH_PORTRAIT 240
#define SCREEN_HEIGHT_PORTRAIT 320
#define SCREEN_WIDTH 320    // Largura em paisagem
#define SCREEN_HEIGHT 240   // Altura em paisagem

// Display - Pinos de controle:
#define TFT_CS    5 
#define TFT_RST   15
#define TFT_DC    4
#define TFT_MOSI  23   // SDA, HW MOSI
#define TFT_SCLK  18   // SCL, HW SCLK

// Configuração SPI ULTRA otimizada para performance do display:
#define SPI_FREQUENCY 40000000        // 40MHz - frequência segura e otimizada para ST7789
#define SPI_READ_FREQUENCY 20000000   // 20MHz - frequência segura para leituras

// Inicialização do display com SPI hardware otimizado:
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// WiFi: Credenciais (Primária e Backup)
const char* ssid_primary = "ANDROMEDA";      // SSID principal
const char* pass_primary = "22602260";       // Senha principal
const char* ssid_backup  = "spectrum_01";    // SSID backup
const char* pass_backup  = "22602260";       // Senha backup

// Dados do MQTT: Aqui vão os tópicos via mqtt tradicional para o broker HIVEMQ
// Ajuste o broker e os tópicos conforme sua necessidade
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* clientID = "cangaco_01";
const char* topicChuva = "est_01/chuva";
const char* topicTemp = "est_01/temp";
const char* topicUmid = "est_01/umid";
const char* topicSolar = "est_01/solar";
const char* topicAlerta = "est_01/alerta";
const char* topicDados = "est_01/dados";

// Configuração API ThingSpeak: Insira abaixo suas credenciais
const char* ts_server = "api.thingspeak.com";
const char* writeAPIKey = "2PI8MD4NVFEY9XSZ";
const unsigned long channelID = 3140279;

WiFiClient espClient;
PubSubClient client(espClient);

// Configuração trigger alerta de temperatura:
const float TEMP_LIMITE = 30.00;  // Temperatura Limite
const float TEMP_OFFSET = -2.0;   // Offset para correção da temperatura
unsigned long lastMsg = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 3000;    // Atualiza display a cada 3s (mais estável)
const unsigned long MQTT_UPDATE_INTERVAL = 15000;     // Envia dados MQTT a cada 15s

// Variáveis para otimização do display:
float lastTemp = -999, lastUmid = -999, lastInsolacao = -999;
bool lastChovendo = false, lastAlerta = false;
bool forceDisplayUpdate = true;

// Controle de reconexão Wifi automática:
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30s
bool wifiConnected = false;
bool mqttConnected = false;

// Controle de qual rede está sendo usada:
bool usingPrimaryNetwork = false;
bool usingBackupNetwork = false;

// NTP para timestamp: Sincronização de horário para timestamp
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; 
const int daylightOffset_sec = 0;

// Funções auxiliares:
void beepDouble() {
  digitalWrite(BUZZER, HIGH); delay(120); digitalWrite(BUZZER, LOW); delay(120);
  digitalWrite(BUZZER, HIGH); delay(120); digitalWrite(BUZZER, LOW);
}

String getTimeStamp() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "0";
  }
  time(&now);
  return String((unsigned long)now);
}

// Função para resetar flags estáticas (útil para mudanças de rede):
void resetDisplayFlags() {
  // Esta função pode ser chamada quando há mudança significativa de estado
  forceDisplayUpdate = true;
}

// Função para limpar área específica (ULTRA otimizada):
void clearDisplayArea(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, ST77XX_BLACK);  
}

// Funções auxiliares para centralização no display:
int getCenterX(String text, int textSize) {
  int charWidth = 6 * textSize;
  int textWidth = text.length() * charWidth;
  int centerX = (SCREEN_WIDTH - textWidth) / 2;
  return centerX < 0 ? 0 : centerX;
}

int getCenterY(int totalHeight) {
  int centerY = (SCREEN_HEIGHT - totalHeight) / 2;
  return centerY < 0 ? 0 : centerY;
}

void drawCenteredText(String text, int x, int y, int size, uint16_t color) {
  if (size < 1) size = 1;
  if (size > 4) size = 4;
  
  tft.setTextSize(size);
  tft.setTextColor(color, ST77XX_BLACK);
  
  if (x == -1) {
    x = getCenterX(text, size);
  }
  
  // Garante que as coordenadas estão dentro dos limites da tela:
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    tft.setCursor(x, y);
    tft.print(text);
  }
}

// Tela de boas-vindas otimizada:
void showWelcomeScreen() {
  Serial.println("[INFO] Exibindo tela de boas-vindas...");
  
  // Otimiza configurações do display:
  tft.cp437(true);          // Usa conjunto de caracteres padrão
  tft.setTextWrap(false);   // Desabilita quebra automática de texto
  tft.fillScreen(ST77XX_BLACK);
  
  // Cálculo para centralização vertical total (layout para 320x240):
  int totalTextHeight = 4 * 30;
  int startY = getCenterY(totalTextHeight);
  
  // Logo/Título principal centralizado:
  drawCenteredText("Estacao", -1, startY, 2, ST77XX_WHITE);
  drawCenteredText("Meteorologica", -1, startY + 30, 2, ST77XX_CYAN);
  
  // Linha decorativa:
  int lineY = startY + 70;
  tft.drawFastHLine(SCREEN_WIDTH/4, lineY, SCREEN_WIDTH/2, ST77XX_BLUE);
  
  // Nome do projeto e identificação:
  drawCenteredText("CANGACEIROS", -1, lineY + 20, 2, ST77XX_YELLOW);
  drawCenteredText("[ est_01 ]", -1, lineY + 50, 2, ST77XX_GREEN); //
  
  // Indicador de carregamento:
  for(int i = 0; i < 4; i++) {
    int dotX = (SCREEN_WIDTH/2) - 30 + (i * 20);
    tft.fillCircle(dotX, lineY + 75, 3, ST77XX_WHITE);
    delay(300);
  }
  
  Serial.println("[INFO] Tela de boas-vindas exibida");
  delay(2000);
}

// Função para desenhar barra de carregamento WiFi otimizada:
void drawWiFiLoadingScreen(String ssid, int progress, int maxProgress) {
  // Limpa apenas a área necessária em vez da tela toda
  static bool firstRun = true;
  if (firstRun) {
    tft.fillScreen(ST77XX_BLACK);
    // Configura parâmetros básicos
    tft.cp437(true);
    tft.setTextWrap(false);
    firstRun = false;
  } else {
    // Limpa apenas a área da barra de progresso e porcentagem
    clearDisplayArea(0, 95, SCREEN_WIDTH, 60);
  }
  
  // Título (só desenha na primeira vez):
  static bool titleDrawn = false;
  if (!titleDrawn) {
    drawCenteredText("CONECTANDO WIFI", -1, 40, 2, ST77XX_CYAN);
    String redeText = "Rede: " + ssid;
    drawCenteredText(redeText, -1, 70, 2, ST77XX_WHITE);
    titleDrawn = true;
  }
  
  // Barra de carregamento: Tamanho
  int barWidth = 200;
  int barHeight = 12;
  int barX = (SCREEN_WIDTH - barWidth) / 2;
  int barY = 100;
  
  // Moldura da barra:
  tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
  
  // Preenchimento da barra:
  int fillWidth = (progress * (barWidth - 2)) / maxProgress;
  if (fillWidth > 0) {
    tft.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, ST77XX_GREEN);
  }
  
  // Porcentagem:
  int percent = (progress * 100) / maxProgress;
  String percentStr = String(percent) + "%";
  drawCenteredText(percentStr, -1, 125, 1, ST77XX_YELLOW);
  
  // Status:
  drawCenteredText("Tentando conectar...", -1, 150, 1, ST77XX_WHITE);
  drawCenteredText("Aguarde", -1, 180, 1, ST77XX_GREEN);
}

// Conexão WiFi com barra de carregamento simplificada:
bool connectWifiWithProgress() {
  // Mostra tela inicial de conexão
  tft.fillScreen(ST77XX_BLACK);
  drawCenteredText("CONECTANDO WIFI", -1, 40, 2, ST77XX_CYAN);
  
  // Tenta rede primária
  Serial.print("[WiFi] Conectando à rede primária: ");
  Serial.println(ssid_primary);
  
  String redeText = "Rede: " + String(ssid_primary);
  drawCenteredText(redeText, -1, 70, 1, ST77XX_WHITE);
  drawCenteredText("Tentando conectar...", -1, 100, 1, ST77XX_YELLOW);
  
  WiFi.begin(ssid_primary, pass_primary);
  
  // Barra de carregamento simplificada para rede primária:
  for (int i = 0; i <= 20; i++) {
    // Desenha barra simples
    int barWidth = 200;
    int barHeight = 8;
    int barX = (SCREEN_WIDTH - barWidth) / 2;
    int barY = 120;
    
    // Apaga barra anterior
    tft.fillRect(barX, barY, barWidth, barHeight, ST77XX_BLACK);
    // Desenha moldura
    tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
    // Desenha progresso
    int fillWidth = (i * (barWidth - 2)) / 20;
    if (fillWidth > 0) {
      tft.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, ST77XX_GREEN);
    }
    
    // Mostra porcentagem:
    String percent = String((i * 100) / 20) + "%";
    tft.fillRect(barX, barY + 15, barWidth, 20, ST77XX_BLACK); // Limpa área do texto
    drawCenteredText(percent, -1, barY + 20, 1, ST77XX_YELLOW);
    
    if (WiFi.status() == WL_CONNECTED) {
      beepDouble();
      // Tela de sucesso
      tft.fillScreen(ST77XX_BLACK);
      drawCenteredText("WIFI CONECTADO!", -1, 100, 2, ST77XX_GREEN);
      drawCenteredText(WiFi.SSID(), -1, 130, 1, ST77XX_WHITE);
      drawCenteredText(WiFi.localIP().toString(), -1, 150, 1, ST77XX_YELLOW);
      delay(2000);
      
      Serial.print("[WiFi] Conectado à: "); Serial.println(WiFi.SSID());
      Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
      wifiConnected = true;
      usingPrimaryNetwork = true;
      usingBackupNetwork = false;
      return true;
    }
    delay(250);
  }

  // Tenta rede backup:
  Serial.print("[WiFi] Falha na primária. Tentando rede backup: ");
  Serial.println(ssid_backup);
  WiFi.disconnect(true);
  delay(300);
  
  // Mostra tentativa de backup:
  tft.fillScreen(ST77XX_BLACK);
  drawCenteredText("TENTANDO BACKUP", -1, 40, 2, ST77XX_CYAN);
  String redeBackup = "Rede: " + String(ssid_backup);
  drawCenteredText(redeBackup, -1, 70, 1, ST77XX_WHITE);
  drawCenteredText("Conectando...", -1, 100, 1, ST77XX_YELLOW);
  
  WiFi.begin(ssid_backup, pass_backup);
  
  // Barra de carregamento simplificada para rede backup:
  for (int i = 0; i <= 20; i++) {
    // Desenha barra simples
    int barWidth = 200;
    int barHeight = 8;
    int barX = (SCREEN_WIDTH - barWidth) / 2;
    int barY = 120;
    
    // Apaga barra anterior
    tft.fillRect(barX, barY, barWidth, barHeight, ST77XX_BLACK);
    // Desenha moldura
    tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
    // Desenha progresso
    int fillWidth = (i * (barWidth - 2)) / 20;
    if (fillWidth > 0) {
      tft.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, ST77XX_GREEN);
    }
    
    // Mostra porcentagem
    String percent = String((i * 100) / 20) + "%";
    tft.fillRect(barX, barY + 15, barWidth, 20, ST77XX_BLACK);
    drawCenteredText(percent, -1, barY + 20, 1, ST77XX_YELLOW);
    
    if (WiFi.status() == WL_CONNECTED) {
      beepDouble();
      // Tela de sucesso:
      tft.fillScreen(ST77XX_BLACK);
      drawCenteredText("WIFI CONECTADO!", -1, 100, 2, ST77XX_GREEN);
      drawCenteredText(WiFi.SSID(), -1, 130, 1, ST77XX_WHITE);
      drawCenteredText(WiFi.localIP().toString(), -1, 150, 1, ST77XX_YELLOW);
      delay(2000);
      
      Serial.print("[WiFi] Conectado à: "); Serial.println(WiFi.SSID());
      Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
      wifiConnected = true;
      usingPrimaryNetwork = false;
      usingBackupNetwork = true;
      return true;
    }
    delay(250);
  }

  // Falha em ambas as redes:
  tft.fillScreen(ST77XX_BLACK);
  drawCenteredText("WIFI FALHOU", -1, 100, 2, ST77XX_RED);
  drawCenteredText("Continuando OFF-LINE", -1, 130, 2, ST77XX_YELLOW);
  drawCenteredText("Sistema operacional", -1, 150, 1, ST77XX_WHITE);
  delay(3000);
  
  Serial.println("[WiFi] Não foi possível conectar em nenhuma rede.");
  wifiConnected = false;
  usingPrimaryNetwork = false;
  usingBackupNetwork = false;
  return false;
}

// Função de reconexão automática silenciosa:
bool attemptReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Tentativa de reconexão automática...");
    
    // Tenta reconectar à rede que estava sendo usada antes
    const char* ssid_to_use = usingPrimaryNetwork ? ssid_primary : ssid_backup;
    const char* pass_to_use = usingPrimaryNetwork ? pass_primary : pass_backup;
    
    // Se nenhuma rede estava sendo usada, tenta primária primeiro
    if (!usingPrimaryNetwork && !usingBackupNetwork) {
      ssid_to_use = ssid_primary;
      pass_to_use = pass_primary;
    }
    
    Serial.print("[WiFi] Tentando reconectar à: ");
    Serial.println(ssid_to_use);
    
    WiFi.begin(ssid_to_use, pass_to_use);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Reconectado com sucesso");
      wifiConnected = true;
      // Não muda as flags de rede aqui, mantém a rede atual
      return true;
    } else {
      Serial.println("[WiFi] Falha na reconexão");
      wifiConnected = false;
      usingPrimaryNetwork = false;
      usingBackupNetwork = false;
    }
  } else {
    wifiConnected = true;
  }
  
  // Tenta reconectar MQTT se WiFi estiver conectado:
  if (wifiConnected && !client.connected()) {
    Serial.println("[MQTT] Tentativa de reconexão automática...");
    if (client.connect(clientID)) {
      Serial.println("[MQTT] Reconectado com sucesso!");
      mqttConnected = true;
    } else {
      Serial.println("[MQTT] Falha na reconexão");
      mqttConnected = false;
    }
  }
  
  return wifiConnected;
}

// Função para verificar se os dados mudaram significativamente:
bool needsDisplayUpdate(float temp, float umid, bool chovendo, float insolacao, bool alertaAtivo) {
  // Primeira execução sempre atualiza
  if (forceDisplayUpdate) {
    forceDisplayUpdate = false;
    return true;
  }
  
  // Se os últimos valores são padrão (-999), força atualização:
  if (lastTemp == -999 || lastUmid == -999) {
    return true;
  }
  
  // Verifica mudanças significativas para atualização estável (reduzido para evitar piscar)
  if (abs(temp - lastTemp) > 0.5 ||            // Menos sensível a mudanças de temperatura
      abs(umid - lastUmid) > 2.0 ||             // Menos sensível a mudanças de umidade
      abs(insolacao - lastInsolacao) > 5.0 ||   // Menos sensível a mudanças de luminosidade
      chovendo != lastChovendo ||
      alertaAtivo != lastAlerta) {
    return true;
  }
  return false;
}

// Reconecta MQTT:
void reconnectMQTT() {
  if (wifiConnected && !client.connected()) {
    if (client.connect(clientID)) {
      Serial.println("[INFO] MQTT conectado!");
      mqttConnected = true;
    } else {
      Serial.println("[ERROR] Falha na conexão MQTT");
      mqttConnected = false;
    }
  }
}

// Função para atualização seletiva de valores sem piscar
void updateDisplayValues(float temp, float umid, bool chovendo, float insolacao) {
  // Atualiza apenas os valores que mudaram sem limpar a tela toda
  
  // Temperatura (se mudou significativamente)
  if (abs(temp - lastTemp) > 0.5) {
    tft.fillRect(0, 45, SCREEN_WIDTH, 25, ST77XX_BLACK); // Limpa só a área do valor
    String tempStr = String(temp, 1) + " C";
    drawCenteredText(tempStr, -1, 45, 2, ST77XX_YELLOW);
  }
  
  // Umidade (se mudou significativamente)
  if (abs(umid - lastUmid) > 2.0) {
    tft.fillRect(0, 90, SCREEN_WIDTH, 25, ST77XX_BLACK); // Limpa só a área do valor
    String umidStr = String(umid, 1) + " %";
    drawCenteredText(umidStr, -1, 90, 2, ST77XX_YELLOW);
  }
  
  // Chuva (se mudou)
  if (chovendo != lastChovendo) {
    tft.fillRect(0, 135, SCREEN_WIDTH, 25, ST77XX_BLACK); // Limpa só a área do valor
    String chuvaStr = chovendo ? "CHOVENDO" : "SEM CHUVA";
    uint16_t chuvaColor = chovendo ? ST77XX_GREEN : ST77XX_YELLOW;
    drawCenteredText(chuvaStr, -1, 135, 2, chuvaColor);
  }
  
  // Luminosidade (se mudou significativamente)
  if (abs(insolacao - lastInsolacao) > 5.0) {
    tft.fillRect(0, 180, SCREEN_WIDTH, 25, ST77XX_BLACK); // Limpa só a área do valor
    String luxStr = String(insolacao, 0) + " %";
    drawCenteredText(luxStr, -1, 180, 2, ST77XX_YELLOW);
  }
  
  // Atualiza status das conexões (área inferior)
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
  String mqttStatus = client.connected() ? "MQTT: OK" : "MQTT: OFF";
  String statusLine = wifiStatus + "  |  " + mqttStatus;
  
  tft.fillRect(0, 210, SCREEN_WIDTH, 30, ST77XX_BLACK); // Limpa só a área de status
  drawCenteredText(statusLine, -1, 215, 2, ST77XX_WHITE);
}

// Exibição normal estabilizada:
void drawNormalScreen(float temp, float umid, bool chovendo, float insolacao) {
  Serial.println("[DEBUG] Desenhando tela normal estável");
  
  // Verifica se é necessário redesenhar a tela completa
  static bool screenInitialized = false;
  bool needFullRedraw = !screenInitialized || forceDisplayUpdate;
  
  if (needFullRedraw) {
    // Redesenha tela completa apenas quando necessário
    tft.cp437(true);
    tft.setTextWrap(false);
    tft.fillScreen(ST77XX_BLACK);
    
    // Elementos fixos (só desenha uma vez)
    drawCenteredText("ESTACAO CANGACEIROS", -1, 8, 1, ST77XX_CYAN);
    tft.drawFastHLine(SCREEN_WIDTH/6, 22, 2*SCREEN_WIDTH/3, ST77XX_BLUE);
    
    // Labels fixos
    drawCenteredText("TEMPERATURA", -1, 30, 1, ST77XX_WHITE);
    drawCenteredText("UMIDADE", -1, 75, 1, ST77XX_WHITE);
    drawCenteredText("CHUVA", -1, 120, 1, ST77XX_WHITE);
    drawCenteredText("LUMINOSIDADE", -1, 165, 1, ST77XX_WHITE);
    
    screenInitialized = true;
    
    // Desenha todos os valores na primeira vez
    String tempStr = String(temp, 1) + " C";
    drawCenteredText(tempStr, -1, 45, 2, ST77XX_YELLOW);
    
    String umidStr = String(umid, 1) + " %";
    drawCenteredText(umidStr, -1, 90, 2, ST77XX_YELLOW);
    
    String chuvaStr = chovendo ? "CHOVENDO" : "SEM CHUVA";
    uint16_t chuvaColor = chovendo ? ST77XX_GREEN : ST77XX_YELLOW;
    drawCenteredText(chuvaStr, -1, 135, 2, chuvaColor);
    
    String luxStr = String(insolacao, 0) + " %";
    drawCenteredText(luxStr, -1, 180, 2, ST77XX_YELLOW);
    
    String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
    String mqttStatus = client.connected() ? "MQTT: OK" : "MQTT: OFF";
    String statusLine = wifiStatus + "  |  " + mqttStatus;
    drawCenteredText(statusLine, -1, 215, 2, ST77XX_WHITE);
  } else {
    // Atualização seletiva apenas dos valores que mudaram
    updateDisplayValues(temp, umid, chovendo, insolacao);
  }
}

// Exibição alerta estabilizada:
void drawAlertScreen(float temp, bool chovendo, float insolacao) {
  Serial.println("[DEBUG] Desenhando tela de alerta estável");
  
  // Sempre redesenha tela de alerta completamente (é menos frequente)
  tft.cp437(true);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  
  // Título de alerta
  drawCenteredText("ALERTA", -1, 10, 2, ST77XX_RED);
  drawCenteredText("TEMPERATURA", -1, 35, 1, ST77XX_YELLOW);
  
  // Linha divisória
  tft.drawFastHLine(SCREEN_WIDTH/6, 50, 2*SCREEN_WIDTH/3, ST77XX_WHITE);
  
  // Temperatura crítica
  drawCenteredText("TEMP. CRITICA", -1, 60, 1, ST77XX_WHITE);
  String tempStr = String(temp, 1) + " C";
  drawCenteredText(tempStr, -1, 75, 2, ST77XX_RED);
  
  // Limite excedido
  String limiteStr = "LIMITE: " + String(TEMP_LIMITE, 1) + " C";
  drawCenteredText(limiteStr, -1, 105, 1, ST77XX_WHITE);
  
  // Outras condições
  drawCenteredText("OUTRAS CONDICOES", -1, 125, 1, ST77XX_WHITE);
  
  // Chuva
  drawCenteredText("CHUVA", -1, 140, 1, ST77XX_WHITE);
  String chuvaStr = chovendo ? "CHOVENDO" : "SEM CHUVA";
  uint16_t chuvaColor = chovendo ? ST77XX_GREEN : ST77XX_YELLOW;
  drawCenteredText(chuvaStr, -1, 155, 1, chuvaColor);
  
  // Luminosidade
  drawCenteredText("LUMINOSIDADE", -1, 175, 1, ST77XX_WHITE);
  String luxStr = String(insolacao, 0) + " %";
  drawCenteredText(luxStr, -1, 190, 1, ST77XX_YELLOW);
  
  // Aviso final
  drawCenteredText("!!! ATENCAO !!!", -1, 190, 2, ST77XX_RED);
  
  // Status das conexões no rodapé
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
  String mqttStatus = client.connected() ? "MQTT: OK" : "MQTT: OFF";
  String statusLine = wifiStatus + "  |  " + mqttStatus;
  drawCenteredText(statusLine, -1, 220, 1, ST77XX_WHITE);
}

void setup() {
  // Configuração dos pinos
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  // Desliga LEDs e buzzer inicialmente
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(BUZZER, LOW);
  
  Serial.begin(115200);
  delay(1000); // Aguarda estabilizar
  Serial.println("\n[INFO] === Iniciando Sistema Estacao Meteorologica ===");
  
  // Inicializa DHT
  dht.begin();
  Serial.println("[INFO] Sensor DHT inicializado");
  
  // Display não possui controle de backlight definido
  delay(100);
  Serial.println("[INFO] Preparando inicialização do display");
  
  // Configura SPI HARDWARE com máxima performance:
  SPI.begin();  
  SPI.setFrequency(SPI_FREQUENCY);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  // Inicialização ULTRA otimizada do display ST7789 GMT020-02-7P v1.3:
  tft.init(SCREEN_WIDTH_PORTRAIT, SCREEN_HEIGHT_PORTRAIT, SPI_MODE0);
  delay(50);  
  
  // Define rotação para paisagem ANTES de qualquer operação gráfica
  tft.setRotation(1); // Paisagem (0°=retrato, 1=paisagem, 2=retrato invertido, 3=paisagem invertida)
  delay(50);
  tft.fillScreen(ST77XX_BLACK);
  
  // Aplica configurações básicas de performance ao display
  tft.cp437(true);  
  tft.setTextWrap(false);  
  
  // Configuração adicional de performance para ESP32
  #ifdef ESP32
  #endif
  
  // Exibe tela de boas-vindas:
  showWelcomeScreen();
  
  // Pequeno delay para estabilizar display
  delay(500);
  
  // Força primeira atualização do display após boas-vindas
  forceDisplayUpdate = true;
  lastDisplayUpdate = 0; // Garante que a primeira verificação no loop seja executada
  
  // Conecta WiFi com barra de carregamento
  connectWifiWithProgress();
  
  // Atualiza status das conexões
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // Configura NTP
  if (wifiConnected) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("[INFO] NTP configurado");
  }
  
  // Configura MQTT
  client.setServer(mqtt_server, mqtt_port);
  if (wifiConnected) {
    reconnectMQTT();
  }
  mqttConnected = client.connected();
  Serial.println("[INFO] MQTT configurado");
  
  // Inicializa Blynk com as credenciais da rede que está conectada
  if (wifiConnected) {
    if (usingPrimaryNetwork) {
      Blynk.begin(BLYNK_AUTH_TOKEN, ssid_primary, pass_primary);
      Serial.println("[INFO] Blynk inicializado com rede primária");
    } else if (usingBackupNetwork) {
      Blynk.begin(BLYNK_AUTH_TOKEN, ssid_backup, pass_backup);
      Serial.println("[INFO] Blynk inicializado com rede backup");
    } else {
      // Fallback: usa as credenciais da rede atual
      Serial.println("[INFO] Blynk inicializado com configuração automática");
      Blynk.config(BLYNK_AUTH_TOKEN);
      Blynk.connect();
    }
  } else {
    Serial.println("[INFO] Blynk não inicializado - sem WiFi");
  }
  
  // Sinal de inicialização completa
  digitalWrite(LED_VERDE, HIGH);
  Serial.println("[INFO] === Sistema inicializado com sucesso! ===");

  // Sinal de inicialização completa
  digitalWrite(LED_VERDE, HIGH);
  Serial.println("[INFO] === Sistema inicializado com sucesso! ===");
}

void loop() {
  unsigned long now = millis();
  
  // Reconexão automática a cada 30 segundos
  if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
    attemptReconnect();
    lastReconnectAttempt = now;
  }
  
  // Atualiza status das conexões
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  mqttConnected = client.connected();
  
  // Mantém conexão MQTT ativa
  if (wifiConnected && !client.connected()) {
    reconnectMQTT();
  }
  if (client.connected()) {
    client.loop();
  }
  
  // Executa Blynk se WiFi estiver conectado (sem forçar reconexão)
  if (wifiConnected) {
    // Só tenta reconectar Blynk se realmente perdeu a conexão
    if (!Blynk.connected()) {
      static unsigned long lastBlynkReconnect = 0;
      // Tenta reconectar Blynk apenas a cada 30 segundos
      if (now - lastBlynkReconnect > 30000) {
        Serial.println("[BLYNK] Tentativa de reconexão...");
        Blynk.connect(1000); // Timeout de 1 segundo
        lastBlynkReconnect = now;
      }
    } else {
      Blynk.run();
    }
  }

  // Leitura dos sensores (sempre atualizada)
  float temp = dht.readTemperature() + TEMP_OFFSET; // Aplica offset de correção
  float umid = dht.readHumidity();
  
  // Tratamento de valores inválidos do DHT
  if (isnan(temp - TEMP_OFFSET)) { // Verifica se a leitura original era inválida
    temp = 25.0 + TEMP_OFFSET; // Valor padrão com offset aplicado
  }
  if (isnan(umid)) {
    umid = 50.0; // Valor padrão
  }
  
  int chuvaVal = analogRead(CHUVA_PIN);
  bool chovendo = chuvaVal < 2000;
  int leituraSolar = analogRead(SOLAR_PIN);
  const float offsetSolar = 0.4;
  float tensaoSolar = (leituraSolar / 4095.0) * 3.3;
  float tensaoSolarCorrigida = tensaoSolar - offsetSolar;
  if (tensaoSolarCorrigida < 0) tensaoSolarCorrigida = 0;
  float insolacaoPercent = (tensaoSolarCorrigida / 4.0) * 100.0;
  if (insolacaoPercent > 100) insolacaoPercent = 100;
  bool alertaAtivo = (temp > TEMP_LIMITE);
  String alerta = alertaAtivo ? "on" : "off";
  
  // Debug das leituras
  static unsigned long lastDebug = 0;
  if (now - lastDebug > 10000) { // Debug a cada 10s
    Serial.println("[DEBUG] Sensores - Temp: " + String(temp) + ", Umid: " + String(umid) + 
                   ", Chuva: " + String(chuvaVal) + ", Solar: " + String(insolacaoPercent));
    lastDebug = now;
  }
  
  // Atualização estabilizada do display (3 segundos para evitar piscar)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    // Sempre atualiza o display para garantir funcionamento
    bool shouldUpdate = needsDisplayUpdate(temp, umid, chovendo, insolacaoPercent, alertaAtivo);
    
    // Força atualização a cada 20 segundos para garantir refresh moderado
    static unsigned long lastForceUpdate = 0;
    if (now - lastForceUpdate > 20000) {
      shouldUpdate = true;
      lastForceUpdate = now;
      Serial.println("[DEBUG] Forçando atualização periódica do display");
    }
    
    if (shouldUpdate) {
      Serial.println("[DEBUG] Atualizando display - Alerta: " + String(alertaAtivo));
      
      if (alertaAtivo) {
        drawAlertScreen(temp, chovendo, insolacaoPercent);
      } else {
        drawNormalScreen(temp, umid, chovendo, insolacaoPercent);
      }
      
      // Armazena valores atuais:
      lastTemp = temp;
      lastUmid = umid;
      lastInsolacao = insolacaoPercent;
      lastChovendo = chovendo;
      lastAlerta = alertaAtivo;
    }
    lastDisplayUpdate = now;
  }
  
  // Controle de LEDs e buzzer:
  if (alertaAtivo) {
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    // Buzzer sem delay bloqueante
    static unsigned long lastBeep = 0;
    if (now - lastBeep > 2000) {  // Beep a cada 2s
      digitalWrite(BUZZER, HIGH);
      delay(50);                  // Delay muito curto para beep
      digitalWrite(BUZZER, LOW);
      lastBeep = now;
    }
  } else {
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(BUZZER, LOW);
  }
  
  // Envio de dados MQTT e ThingSpeak (5 segundos)
  if (now - lastMsg >= MQTT_UPDATE_INTERVAL) {
    lastMsg = now;
    String timestamp = getTimeStamp();

    // Publicações individuais MQTT:
    if (!isnan(temp)) client.publish(topicTemp, String(temp).c_str());
    if (!isnan(umid)) client.publish(topicUmid, String(umid).c_str());
    client.publish(topicChuva, chovendo ? "Chovendo" : "Sem chuva");
    client.publish(topicSolar, String(insolacaoPercent, 0).c_str());
    client.publish(topicAlerta, alerta.c_str());

    // Envio para Blynk (Virtual Pins):
    if (wifiConnected && Blynk.connected()) {
      Blynk.virtualWrite(V0, temp);                   // V0 = Temperatura
      Blynk.virtualWrite(V1, umid);                   // V1 = Umidade  
      Blynk.virtualWrite(V2, insolacaoPercent);       // V2 = Luminosidade
      Blynk.virtualWrite(V3, chovendo ? 1 : 0);       // V3 = Chuva (0/1)
      Blynk.virtualWrite(V4, alertaAtivo ? 1 : 0);    // V4 = Alerta (0/1)
      Serial.println("[BLYNK] Dados enviados para Blynk");
    }

    // Envio para ThingSpeak:
    if (!isnan(temp) && !isnan(umid)) {
      String tsUrl = "/update?api_key=";
      tsUrl += writeAPIKey;
      tsUrl += "&field1=";
      tsUrl += String(temp);
      tsUrl += "&field2=";
      tsUrl += String(umid);
      tsUrl += "&field3=";
      tsUrl += String(insolacaoPercent);
      
      WiFiClient tsClient;
      if (tsClient.connect(ts_server, 80)) {
        tsClient.print(String("GET ") + tsUrl + " HTTP/1.1\r\n" +
                      "Host: " + ts_server + "\r\n" +
                      "Connection: close\r\n\r\n");
        // Removido delay(100) para não bloquear
        tsClient.stop();
      }
    }

    // JSON consolidado:
    String payload = "{";
    payload += "\"Temperatura\": " + String(temp, 1) + ",";
    payload += "\"Umidade\": " + String(umid, 1) + ",";
    payload += "\"Lux\": " + String(insolacaoPercent, 0) + ",";
    payload += "\"Chuva\": \"" + String(chovendo ? "Chovendo" : "Sem chuva") + "\",";
    payload += "\"Alerta Temp.\": \"" + alerta + "\",";
    payload += "\"Timestamp\": \"" + timestamp + "\"";
    payload += "}";
    client.publish(topicDados, payload.c_str());
    Serial.println(payload);
  }
  
  // Delay moderado para não sobrecarregar o processador mantendo boa responsividade
  delay(10);  // 10ms - bom equilíbrio entre performance e estabilidade
}

