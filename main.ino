/* 
  Código ajustado para teste de dashboard no Thinkspeak.com
  A conexão para dados junto ao thnkspeak foi feita via API.
  Observe os dados relativos à conexão após linha de comentários
  do thinkspeak.
*/

// Bibliotecas:
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <DHT.h>
#include <time.h>

// Hardware:
#define DHTPIN 32
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define LED_VERDE 15
#define LED_VERMELHO 16
#define BUZZER 2
#define CHUVA_PIN 34
#define SOLAR_PIN 33  

// Display:
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// WiFi: Credenciais
const char* ssid = "**********"; // <- set your SSID here
const char* password = "**********"; // <- Set your WIFI password here

// Dados do MQTT:
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* clientID = "cangaco_01";
const char* topicChuva = "est_01/chuva";
const char* topicTemp = "est_01/temp";
const char* topicUmid = "est_01/umid";
const char* topicSolar = "est_01/solar";
const char* topicAlerta = "est_01/alerta";
const char* topicDados = "est_01/dados";

// Configuração ThingSpeak
const char* ts_server = "api.thingspeak.com";
const char* writeAPIKey = "2PI8MD4NVFEY9XSZ";
const unsigned long channelID = 3140279;

WiFiClient espClient;
PubSubClient client(espClient);

// Configuração:
const float TEMP_LIMITE = 25.00;
unsigned long lastMsg = 0;
bool alertaTemperatura = false;

// NTP para timestamp:
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; // <- fuso horário de Brasília
const int daylightOffset_sec = 0;

// Funções auxiliares:
String getTimeStamp() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "0";
  }
  time(&now);
  return String((unsigned long)now);
}

// Tela de boas-vindas:
void showWelcomeScreen() {
  display.clearDisplay();
  u8g2Fonts.setFont(u8g2_font_helvR08_tr);
  const char* linha1 = "Estacao Meteorologica";
  const char* linha2 = "Cangaceiros";
  const char* linha3 = "est_01";
  int16_t x, y; uint16_t w, h;
  int alturaTotalTexto = 36; 
  int y_pos = (SCREEN_HEIGHT - alturaTotalTexto) / 2;
  display.getTextBounds(linha1, 0, 0, &x, &y, &w, &h);
  u8g2Fonts.setCursor((SCREEN_WIDTH - w) / 2, y_pos);
  u8g2Fonts.print(linha1);
  display.getTextBounds(linha2, 0, 0, &x, &y, &w, &h);
  u8g2Fonts.setCursor((SCREEN_WIDTH - w) / 2, y_pos + 14);
  u8g2Fonts.print(linha2);
  display.getTextBounds(linha3, 0, 0, &x, &y, &w, &h);
  u8g2Fonts.setCursor((SCREEN_WIDTH - w) / 2, y_pos + 28);
  u8g2Fonts.print(linha3);
  display.display();
  delay(5000); 
}

// Conexão WiFi:
bool connectWifi() {
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 60) {
    delay(250);
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
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
  display.clearDisplay();
  const char* cabecalho = "Estacao Cangaceiros";
  u8g2Fonts.setFont(u8g2_font_helvR08_tr);
  u8g2Fonts.setCursor(0, 12); u8g2Fonts.print(cabecalho);
  u8g2Fonts.setCursor(0, 28);
  u8g2Fonts.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    u8g2Fonts.print(WiFi.localIP());
  } else {
    u8g2Fonts.print("off");
  }
  u8g2Fonts.setCursor(0, 40);
  u8g2Fonts.print("Temp: "); u8g2Fonts.print(temp); u8g2Fonts.print(" C");
  u8g2Fonts.setCursor(0, 52);
  u8g2Fonts.print("Umid: "); u8g2Fonts.print(umid); u8g2Fonts.print(" %");
  u8g2Fonts.setCursor(0, 64);
  u8g2Fonts.print("Chuva: "); u8g2Fonts.print(chovendo ? "Sim" : "Nao");
  u8g2Fonts.setCursor(80, 52);
  u8g2Fonts.print("Luz: "); u8g2Fonts.print(insolacao, 0); u8g2Fonts.print("%");
  display.display();
}

// Exibição alerta:
void drawAlertScreen(float temp, bool chovendo, float insolacao) {
  display.clearDisplay();
  u8g2Fonts.setFont(u8g2_font_fub14_tr);
  const char* alerta = "ALERTA";
  u8g2Fonts.setCursor(25, 14);
  u8g2Fonts.print(alerta);
  char temp_str[20];
  sprintf(temp_str, "%.1f oC", temp);
  u8g2Fonts.setCursor(28, 40);
  u8g2Fonts.print(temp_str);
  u8g2Fonts.setFont(u8g2_font_helvR08_tr);
  u8g2Fonts.setCursor(5, SCREEN_HEIGHT - 6);
  u8g2Fonts.print("Chuva: "); u8g2Fonts.print(chovendo ? "Sim" : "Nao");
  u8g2Fonts.setCursor(80, SCREEN_HEIGHT - 6);
  u8g2Fonts.print("Sol: "); u8g2Fonts.print(insolacao, 0); u8g2Fonts.print("%");
  display.display();
}

void setup() {
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  Serial.begin(115200);
  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  u8g2Fonts.begin(display);  
  u8g2Fonts.setFont(u8g2_font_helvR08_tr);
  showWelcomeScreen();
  connectWifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    float temp = dht.readTemperature();
    float umid = dht.readHumidity();
    int chuvaVal = analogRead(CHUVA_PIN);
    bool chovendo = chuvaVal < 2000;
    int leituraSolar = analogRead(SOLAR_PIN);
    const float offsetSolar = 0.4;      
    float tensaoSolar = (leituraSolar / 4095.0) * 3.3;
    float tensaoSolarCorrigida = tensaoSolar - offsetSolar;
    if (tensaoSolarCorrigida < 0) tensaoSolarCorrigida = 0;
    float insolacaoPercent = (tensaoSolarCorrigida / 2.0) * 100.0;
    if (insolacaoPercent > 100) insolacaoPercent = 100;
    String alerta = (temp > TEMP_LIMITE) ? "on" : "off";
    String timestamp = getTimeStamp();

    // Publicações individuais
    if (!isnan(temp)) client.publish(topicTemp, String(temp).c_str());
    if (!isnan(umid)) client.publish(topicUmid, String(umid).c_str());
    client.publish(topicChuva, chovendo ? "Chovendo" : "Sem chuva");
    client.publish(topicSolar, String(insolacaoPercent, 0).c_str());
    client.publish(topicAlerta, alerta.c_str());

    // Envio para ThingSpeak
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
        delay(100);
        tsClient.stop();
      }

    // JSON consolidado
    String payload = "{";
    payload += "\"temperatura\": " + String(temp, 1) + ",";
    payload += "\"umidade\": " + String(umid, 1) + ",";
    payload += "\"indice_solar\": " + String(insolacaoPercent, 0) + ",";
    payload += "\"chuva\": \"" + String(chovendo ? "Chovendo" : "Sem chuva") + "\",";
    payload += "\"alerta\": \"" + alerta + "\",";
    payload += "\"timestamp\": \"" + timestamp + "\"";
    payload += "}";
    client.publish(topicDados, payload.c_str());
    Serial.println(payload);

    // LEDs e buzzer
    if (alerta == "on") {
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
    } else {
      digitalWrite(LED_VERMELHO, LOW);
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(BUZZER, LOW);
    }
    // Display
    if (alerta == "on") {
      drawAlertScreen(temp, chovendo, insolacaoPercent);
    } else {
      drawNormalScreen(temp, umid, chovendo, insolacaoPercent);
    }
  }
}
