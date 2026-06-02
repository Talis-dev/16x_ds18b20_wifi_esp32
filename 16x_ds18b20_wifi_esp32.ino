#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

// Definição dos GPIOs para os barramentos
#define ONE_WIRE_BUS_1 23
#define ONE_WIRE_BUS_2 19
#define ONE_WIRE_BUS_3 18
#define ONE_WIRE_BUS_4 5

// GPIOs para LEDs de status
#define LED_1 25
#define LED_2 26
#define LED_3 27
#define LED_4 14
#define LED_5 4

#define BOOT_BUTTON 0
#define BOOT_HOLD_MS 3000

// Instâncias OneWire e DallasTemperature para cada barramento
OneWire oneWire1(ONE_WIRE_BUS_1);
OneWire oneWire2(ONE_WIRE_BUS_2);
OneWire oneWire3(ONE_WIRE_BUS_3);
OneWire oneWire4(ONE_WIRE_BUS_4);

DallasTemperature sensors1(&oneWire1);
DallasTemperature sensors2(&oneWire2);
DallasTemperature sensors3(&oneWire3);
DallasTemperature sensors4(&oneWire4);

// Número de dispositivos por barramento
int numberOfDevices1, numberOfDevices2, numberOfDevices3, numberOfDevices4;
DeviceAddress tempDeviceAddress;

const char* wifiApName     = "Sensors_ds18b20";
const char* wifiApPassword = "ds18b20123";

Config cfg;
String g_busCache[4] = {"[]", "[]", "[]", "[]"};

WiFiClient   espClient;
PubSubClient client(espClient);
WebServer    webServer(80);

unsigned long lastmillis = 0;
bool send_addres = false;           // device list enviado apos 1a conexao MQTT
bool restartRequested = false;      // flag para reinicio solicitado via HTTP
unsigned long restartAt = 0;
unsigned long lastWifiReconnectAttempt = 0;
int wifiReconnectAttempts = 0;
unsigned long lastMqttAttempt = 0;

// Declaracoes antecipadas de reconect.ino
extern bool          mqttFailed;
extern int           reboot;
extern bool          mqttEverConnected;
extern unsigned long g_mqttRetryInterval;
extern void          mqttForceReconnect();


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Inicializando");
  configLoad(cfg);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);

  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  digitalWrite(LED_3, LOW);
  digitalWrite(LED_4, LOW);
  digitalWrite(LED_5, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // Verifica se o botão BOOT está pressionado por 3s para resetar credenciais WiFi
  if (digitalRead(BOOT_BUTTON) == LOW) {
    Serial.println(F("Botao BOOT pressionado. Aguardando 3s para resetar credenciais WiFi..."));
    unsigned long pressStart = millis();
    while (digitalRead(BOOT_BUTTON) == LOW) {
      digitalWrite(LED_5, !digitalRead(LED_5));
      delay(200);
      if (millis() - pressStart >= BOOT_HOLD_MS) {
        Serial.println(F("Resetando credenciais WiFi e abrindo portal de configuracao..."));
        digitalWrite(LED_5, HIGH);
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        wifiManager.setConfigPortalTimeout(240);
        if (!wifiManager.startConfigPortal(wifiApName, wifiApPassword)) {
          Serial.println(F("Portal encerrado sem configuracao. Reiniciando..."));
        }
        delay(500);
        ESP.restart();
      }
    }
    Serial.println(F("Botao solto antes de 3s. Continuando normalmente."));
    digitalWrite(LED_5, HIGH);
  }

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(240);
  wifiManager.setConnectTimeout(30);
  if (!wifiManager.autoConnect(wifiApName, wifiApPassword)) {
    digitalWrite(LED_5, HIGH);
    Serial.println(F("Falha na conexão. Resetar e tentar novamente..."));
    delay(500);
    ESP.restart();
  }

  Serial.println(F("Conectado na rede Wifi."));
  Serial.print(F("Endereço IP: "));
  Serial.println(WiFi.localIP());

  client.setServer(cfg.mqttServer, cfg.mqttPort);
  client.setCallback(callback);
  webServerSetup();
digitalWrite(LED_5, LOW);
  // Inicializar bibliotecas
  sensors1.begin();
  sensors2.begin();
  sensors3.begin();
  sensors4.begin();

  // Contar dispositivos em cada barramento
  numberOfDevices1 = sensors1.getDeviceCount();
  numberOfDevices2 = sensors2.getDeviceCount();
  numberOfDevices3 = sensors3.getDeviceCount();
  numberOfDevices4 = sensors4.getDeviceCount();

  Serial.printf("Barramento 1: %d dispositivos encontrados\n", numberOfDevices1);
  Serial.printf("Barramento 2: %d dispositivos encontrados\n", numberOfDevices2);
  Serial.printf("Barramento 3: %d dispositivos encontrados\n", numberOfDevices3);
  Serial.printf("Barramento 4: %d dispositivos encontrados\n", numberOfDevices4);

delay(100);
}

void handleWiFiDisconnect() {
  digitalWrite(LED_5, HIGH);

  unsigned long now = millis();
  if (now - lastWifiReconnectAttempt < 10000) {
    return;
  }

  lastWifiReconnectAttempt = now;
  wifiReconnectAttempts++;

  Serial.print(F("WiFi desconectado. Tentativa de reconexao "));
  Serial.println(wifiReconnectAttempts);
  WiFi.reconnect();

  if (wifiReconnectAttempts >= 30) {
    Serial.println(F("WiFi nao reconectou. Reiniciando para abrir o WiFiManager se necessario."));
    delay(500);
    ESP.restart();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    handleWiFiDisconnect();
    return;
  }

  wifiReconnectAttempts = 0;

  // Reinicio solicitado via HTTP (aguarda resposta ser enviada antes)
  if (restartRequested && millis() >= restartAt) {
    Serial.println(F("[SYS] Reiniciando por solicitacao HTTP..."));
    delay(200);
    ESP.restart();
  }

  client.loop();
  handleWebClient();

  // Reconexao MQTT com backoff exponencial (sem parar apos N falhas)
  if (!client.connected()) {
    if (millis() - lastMqttAttempt >= g_mqttRetryInterval) {
      lastMqttAttempt = millis();
      reconnect();
    }
  }

  // Envia IDs dos sensores apos primeira conexao MQTT (identifica dispositivos no servidor)
  if (!send_addres && client.connected()) {
    send_addres = true;
    sendDeviceList(sensors1, numberOfDevices1, "sensors_ds18b20/barramento_1");
    sendDeviceList(sensors2, numberOfDevices2, "sensors_ds18b20/barramento_2");
    sendDeviceList(sensors3, numberOfDevices3, "sensors_ds18b20/barramento_3");
    sendDeviceList(sensors4, numberOfDevices4, "sensors_ds18b20/barramento_4");
  }


  if (millis() - lastmillis > cfg.pollInterval) {
    lastmillis = millis();
    digitalWrite(LED_5, HIGH);
    sendTemperaturesWithAddress(sensors1, numberOfDevices1, "barramento_1", LED_1, 0);
    sendTemperaturesWithAddress(sensors2, numberOfDevices2, "barramento_2", LED_2, 1);
    sendTemperaturesWithAddress(sensors3, numberOfDevices3, "barramento_3", LED_3, 2);
    sendTemperaturesWithAddress(sensors4, numberOfDevices4, "barramento_4", LED_4, 3);
    digitalWrite(LED_5, LOW);
  }
}

void sendTemperaturesWithAddress(DallasTemperature& sensors, int numberOfDevices, const char* topic, int ledPin, int busIndex) {
  StaticJsonDocument<256>  doc;
  StaticJsonDocument<1024> cacheDoc;
  JsonArray cacheArr = cacheDoc.to<JsonArray>();
  bool dataInsert = false;

  digitalWrite(LED_5, HIGH);
  digitalWrite(ledPin, HIGH);

  sensors.requestTemperatures();

  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      char addressString[17];
      getAddressAsString(tempDeviceAddress, addressString);

      float tempC = sensors.getTempC(tempDeviceAddress);
      Serial.print("Barramento ");
      Serial.print(topic);
      Serial.print(" - Address: ");
      Serial.print(addressString);
      Serial.print(" - Temp C: ");
      Serial.println(tempC);

      doc[addressString] = tempC;
      JsonObject entry = cacheArr.createNestedObject();
      entry["addr"] = addressString;
      entry["temp"] = tempC;
      dataInsert = true;
    }
  }

  String json;
  if (dataInsert) {
    serializeJson(doc, json);
  } else {
    json = "{\"error\": \"no devices found\"}";
  }

  client.publish(topic, json.c_str());

  serializeJson(cacheArr, g_busCache[busIndex]);

  digitalWrite(ledPin, LOW);
  digitalWrite(LED_5, LOW);
  delay(100);
}




void getAddressAsString(DeviceAddress deviceAddress, char* buffer) {
  for (uint8_t i = 0; i < 8; i++) {
    sprintf(buffer + i * 2, "%02X", deviceAddress[i]);
  }
  buffer[16] = '\0';
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem Recebida no Tópico [");
  Serial.print(topic);
  Serial.print("] ");
  String PayLoad;

  for (int i = 0; i < length; i++) {
    PayLoad += (char)payload[i];
  }
  Serial.println(PayLoad);

  if (strcmp(topic, "reset_ds18b20_esp32") == 0) {
    if (PayLoad.toInt()) {
      ESP.restart();
    }
  }

  if (strcmp(topic, "timeToSend_ds18b20_esp32") == 0) {
    int val = PayLoad.toInt();
    if (val > 0) cfg.pollInterval = (uint32_t)val;
  }
}



void sendDeviceList(DallasTemperature& sensors, int numberOfDevices, const char* topic) {
  StaticJsonDocument<512> doc;
  digitalWrite(LED_5, HIGH);

  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      char addressString[17];
      getAddressAsString(tempDeviceAddress, addressString);  // Função auxiliar para converter o endereço
      doc[addressString] = "detected";  // Marca como detectado
    }
  }

  String json;
  serializeJson(doc, json);
  client.publish(topic, json.c_str());  // Envia o JSON para o tópico MQTT
  digitalWrite(LED_5, LOW);
}
