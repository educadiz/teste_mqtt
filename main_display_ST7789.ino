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
  
  Instale via Library Manager do Arduino IDE.
*/

// Bibliotecas:
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <DHT.h>
#include <time.h>

// Hardware: Pinos de sensores e atuadores
#define DHTPIN 32
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define LED_VERDE 15
#define LED_VERMELHO 16
#define BUZZER 2
#define CHUVA_PIN 34
#define SOLAR_PIN 33  

// Display GMT020-02-7P v1.3 (TFT 2" ST7789):
#define SCREEN_WIDTH_PORTRAIT 240
#define SCREEN_HEIGHT_PORTRAIT 320
#define SCREEN_WIDTH 320    // Largura em paisagem
#define SCREEN_HEIGHT 240   // Altura em paisagem

// display - Pinos de controle
#define TFT_CS    5 
#define TFT_RST   18
#define TFT_DC    4
#define TFT_MOSI  21   // SDA, HW MOSI
#define TFT_SCLK  22   // SCL, HW SCLK

// Inicialização do display:
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// WiFi: Credenciais (Primária e Backup)
const char* ssid_primary = "ANDROMEDA";      // SSID principal
const char* pass_primary = "22602260";       // Senha principal
const char* ssid_backup  = "spectrum_01";    // SSID backup
const char* pass_backup  = "22602260";       // Senha backup

// Dados do MQTT: Aqui vão os tópicos via mqtt tradicional para o broker HIVEMQ
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* clientID = "cangaco_01";
const char* topicChuva = "est_01/chuva";
const char* topicTemp = "est_01/temp";
const char* topicUmid = "est_01/umid";
const char* topicSolar = "est_01/solar";
const char* topicAlerta = "est_01/alerta";
const char* topicDados = "est_01/dados";

// Configuração API ThingSpeak:
const char* ts_server = "api.thingspeak.com";
const char* writeAPIKey = "2PI8MD4NVFEY9XSZ";
const unsigned long channelID = 3140279;

WiFiClient espClient;
PubSubClient client(espClient);

// Configuração trigger alerta de temperatura:
const float TEMP_LIMITE = 30.00;
const float TEMP_OFFSET = -2.0; // Offset para correção da temperatura
unsigned long lastMsg = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 5000;   // Atualiza display a cada 5
const unsigned long MQTT_UPDATE_INTERVAL = 15000;     // Envia dados MQTT a cada 15

// Variáveis para otimização do display:
float lastTemp = -999, lastUmid = -999, lastInsolacao = -999;
bool lastChovendo = false, lastAlerta = false;
bool forceDisplayUpdate = true;

// NTP para timestamp: Sincronização de horário para timestamp
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; 
const int daylightOffset_sec = 0;

// Funções auxiliares:
void beepDouble() {
  // Dois beeps curtos no buzzer ao conectar
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

// Funções auxiliares para centralização:
int getCenterX(String text, int textSize) {
  // Cálculo mais preciso considerando fonte do display
  int charWidth = 6 * textSize; // 6 pixels por caractere é padrão
  int textWidth = text.length() * charWidth;
  int centerX = (SCREEN_WIDTH - textWidth) / 2;
  return centerX < 0 ? 0 : centerX; // Evita valores negativos
}

int getCenterY(int totalHeight) {
  int centerY = (SCREEN_HEIGHT - totalHeight) / 2;
  return centerY < 0 ? 0 : centerY; // Evita valores negativos
}

void drawCenteredText(String text, int x, int y, int size, uint16_t color) {
  // Validação de parâmetros
  if (size < 1) size = 1;
  if (size > 4) size = 4;
  
  tft.setTextSize(size);
  tft.setTextColor(color, ST77XX_BLACK);
  
  if (x == -1) {
    x = getCenterX(text, size); // Auto-centralizar se x = -1
  }
  
  // Garante que as coordenadas estão dentro dos limites da tela
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    tft.setCursor(x, y);
    tft.print(text);
  }
}

// Tela de boas-vindas:
void showWelcomeScreen() {
  Serial.println("[INFO] Exibindo tela de boas-vindas...");
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  
  // Cálculo para centralização vertical total (layout para 320x240)
  int totalTextHeight = 4 * 30; // 4 linhas com espaçamento
  int startY = getCenterY(totalTextHeight);
  
  // Logo/Título principal centralizado
  drawCenteredText("Estacao", -1, startY, 2, ST77XX_WHITE);
  drawCenteredText("Meteorologica", -1, startY + 30, 2, ST77XX_CYAN);
  
  // Linha decorativa
  int lineY = startY + 70;
  tft.drawFastHLine(SCREEN_WIDTH/4, lineY, SCREEN_WIDTH/2, ST77XX_BLUE);
  
  // Nome do projeto e identificação
  drawCenteredText("CANGACEIROS", -1, lineY + 20, 2, ST77XX_YELLOW);
  drawCenteredText("[ est_01 ]", -1, lineY + 50, 2, ST77XX_GREEN); //
  
  // Indicador de carregamento
  for(int i = 0; i < 4; i++) {
    int dotX = (SCREEN_WIDTH/2) - 30 + (i * 20);
    tft.fillCircle(dotX, lineY + 75, 3, ST77XX_WHITE);
    delay(300);
  }
  
  Serial.println("[INFO] Tela de boas-vindas exibida");
  delay(2000);
}

// Conexão WiFi:
bool connectWifi() {
  // Tenta rede primária
  Serial.println("[WiFi] Conectando à rede primária: ANDROMEDA...");
  WiFi.begin(ssid_primary, pass_primary);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 60) { // ~15s
    delay(250);
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    beepDouble();
    Serial.print("[WiFi] Conectado à: "); Serial.println(WiFi.SSID());
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
    return true;
  }

  // Tenta rede backup
  Serial.println("[WiFi] Falha na primária. Tentando rede backup: spectrum_01...");
  WiFi.disconnect(true);
  delay(300);
  WiFi.begin(ssid_backup, pass_backup);
  tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 60) { // ~15s
    delay(250);
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    beepDouble();
    Serial.print("[WiFi] Conectado à: "); Serial.println(WiFi.SSID());
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("[WiFi] Não foi possível conectar em nenhuma rede.");
  return false;
}

// Função para verificar se os dados mudaram significativamente
bool needsDisplayUpdate(float temp, float umid, bool chovendo, float insolacao, bool alertaAtivo) {
  // Primeira execução sempre atualiza
  if (forceDisplayUpdate) {
    forceDisplayUpdate = false;
    return true;
  }
  
  // Se os últimos valores são padrão (-999), força atualização
  if (lastTemp == -999 || lastUmid == -999) {
    return true;
  }
  
  // Verifica mudanças significativas (valores mais baixos para maior sensibilidade)
  if (abs(temp - lastTemp) > 0.1 || 
      abs(umid - lastUmid) > 0.5 ||
      abs(insolacao - lastInsolacao) > 1.0 ||
      chovendo != lastChovendo ||
      alertaAtivo != lastAlerta) {
    return true;
  }
  return false;
}

// Reconecta MQTT:
void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect(clientID)) {
      Serial.println("[INFO] MQTT conectado!");
    } else {
      delay(5000);
    }
  }
}

// Exibição normal:
void drawNormalScreen(float temp, float umid, bool chovendo, float insolacao) {
  Serial.println("[DEBUG] Desenhando tela normal");
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  
  // Título principal (mais compacto)
  drawCenteredText("ESTACAO CANGACEIROS", -1, 8, 1, ST77XX_CYAN);
  
  // Linha divisória
  tft.drawFastHLine(SCREEN_WIDTH/6, 22, 2*SCREEN_WIDTH/3, ST77XX_BLUE);
  
  // Temperatura
  drawCenteredText("TEMPERATURA", -1, 30, 1, ST77XX_WHITE);
  String tempStr = String(temp, 1) + " C";
  drawCenteredText(tempStr, -1, 45, 2, ST77XX_YELLOW);
  
  // Umidade
  drawCenteredText("UMIDADE", -1, 75, 1, ST77XX_WHITE);
  String umidStr = String(umid, 1) + " %";
  drawCenteredText(umidStr, -1, 90, 2, ST77XX_YELLOW);
  
  // Chuva
  drawCenteredText("CHUVA", -1, 120, 1, ST77XX_WHITE);
  String chuvaStr = chovendo ? "CHUVENDO" : "SEM CHUVA";
  uint16_t chuvaColor = chovendo ? ST77XX_GREEN : ST77XX_YELLOW;
  drawCenteredText(chuvaStr, -1, 135, 2, chuvaColor);
  
  // Luminosidade
  drawCenteredText("LUMINOSIDADE", -1, 165, 1, ST77XX_WHITE);
  String luxStr = String(insolacao, 0) + " %";
  drawCenteredText(luxStr, -1, 180, 2, ST77XX_YELLOW);
  
  // Status das conexões no rodapé
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
  uint16_t wifiColor = WiFi.status() == WL_CONNECTED ? ST77XX_GREEN : ST77XX_RED;
  
  String mqttStatus = client.connected() ? "MQTT: OK" : "MQTT: OFF";
  uint16_t mqttColor = client.connected() ? ST77XX_GREEN : ST77XX_RED;
  
  String statusLine = wifiStatus + "  |  " + mqttStatus;
  drawCenteredText(statusLine, -1, 215, 2, ST77XX_WHITE);
  
  // Indicadores coloridos para WiFi e MQTT
  //nt centerX = SCREEN_WIDTH / 2;
  // tft.fillCircle(centerX - 60, 225, 3, wifiColor);  // Indicador WiFi
  // tft.fillCircle(centerX + 60, 225, 3, mqttColor);  // Indicador MQTT
}

// Exibição alerta:
void drawAlertScreen(float temp, bool chovendo, float insolacao) {
  Serial.println("[DEBUG] Desenhando tela de alerta");
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  
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
  String chuvaStr = chovendo ? "CHUVENDO" : "SEM CHUVA";
  uint16_t chuvaColor = chovendo ? ST77XX_BLUE : ST77XX_GREEN;
  drawCenteredText(chuvaStr, -1, 155, 1, chuvaColor);
  
  // Luminosidade
  drawCenteredText("LUMINOSIDADE", -1, 175, 1, ST77XX_WHITE);
  String luxStr = String(insolacao, 0) + " %";
  drawCenteredText(luxStr, -1, 190, 1, ST77XX_YELLOW);
  
  // Aviso final:
  drawCenteredText("!!! ATENCAO !!!", -1, 190, 2, ST77XX_RED);
  
  // Status das conexões no rodapé
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
  String mqttStatus = client.connected() ? "MQTT: OK" : "MQTT: OFF";
  String statusLine = wifiStatus + "  |  " + mqttStatus;
  drawCenteredText(statusLine, -1, 220, 2, ST77XX_WHITE);
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
  
  // Inicializa display com tratamento de erro
  Serial.println("[INFO] Inicializando display ST7789...");
  
  // Inicialização do display ST7789 GMT020-02-7P v1.3
  tft.init(SCREEN_WIDTH_PORTRAIT, SCREEN_HEIGHT_PORTRAIT);
  delay(100);
  
  // Define rotação para paisagem ANTES de qualquer operação gráfica
  tft.setRotation(1); // Paisagem (0°=retrato, 1=paisagem, 2=retrato invertido, 3=paisagem invertida)
  delay(50);
  
  // Agora podemos usar as operações gráficas com segurança
  tft.fillScreen(ST77XX_BLACK);
  
  Serial.println("[INFO] Display ST7789 inicializado com sucesso!");
  Serial.print("[INFO] Resolucao em paisagem: ");
  Serial.print(SCREEN_WIDTH);
  Serial.print("x");
  Serial.println(SCREEN_HEIGHT);
  
  // Exibe tela de boas-vindas
  showWelcomeScreen();
  
  // Força primeira atualização do display após boas-vindas
  forceDisplayUpdate = true;
  lastDisplayUpdate = 0; // Garante que a primeira verificação no loop seja executada
  
  // Conecta WiFi
  Serial.println("[INFO] Iniciando conexao WiFi...");
  connectWifi();
  
  // Configura NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("[INFO] NTP configurado");
  
  // Configura MQTT
  client.setServer(mqtt_server, mqtt_port);
  Serial.println("[INFO] MQTT configurado");
  
  // Sinal de inicialização completa
  digitalWrite(LED_VERDE, HIGH);
  Serial.println("[INFO] === Sistema inicializado com sucesso! ===");
}

void loop() {
  // Mantém conexão MQTT ativa
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  unsigned long now = millis();
  
  // Leitura dos sensores (sempre atualizada)
  float temp = dht.readTemperature() + TEMP_OFFSET; // Aplica offset de correção
  float umid = dht.readHumidity();
  
  // Tratamento de valores inválidos do DHT
  if (isnan(temp - TEMP_OFFSET)) { // Verifica se a leitura original era inválida
    temp = 25.0 + TEMP_OFFSET; // Valor padrão com offset aplicado
    Serial.println("[WARNING] Leitura de temperatura inválida, usando valor padrão");
  }
  if (isnan(umid)) {
    umid = 50.0; // Valor padrão
    Serial.println("[WARNING] Leitura de umidade inválida, usando valor padrão");
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
  
  // Atualização do display (1 segundo)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    // Sempre atualiza o display para garantir funcionamento
    bool shouldUpdate = needsDisplayUpdate(temp, umid, chovendo, insolacaoPercent, alertaAtivo);
    
    // Força atualização a cada 30 segundos mesmo sem mudanças
    static unsigned long lastForceUpdate = 0;
    if (now - lastForceUpdate > 30000) {
      shouldUpdate = true;
      lastForceUpdate = now;
      Serial.println("[DEBUG] Forçando atualização do display");
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
  
  // Controle de LEDs e buzzer (resposta imediata):
  if (alertaAtivo) {
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    // Buzzer sem delay bloqueante
    static unsigned long lastBeep = 0;
    if (now - lastBeep > 2000) { // Beep a cada 2s
      digitalWrite(BUZZER, HIGH);
      delay(50); // Delay muito curto para beep
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

    // Publicações individuais MQTT
    if (!isnan(temp)) client.publish(topicTemp, String(temp).c_str());
    if (!isnan(umid)) client.publish(topicUmid, String(umid).c_str());
    client.publish(topicChuva, chovendo ? "Chovendo" : "Sem chuva");
    client.publish(topicSolar, String(insolacaoPercent, 0).c_str());
    client.publish(topicAlerta, alerta.c_str());

    // Envio para ThingSpeak (apenas com valores válidos)
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

    // JSON consolidado
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
  
  // Pequeno delay para não sobrecarregar o processador
  delay(10);
}

