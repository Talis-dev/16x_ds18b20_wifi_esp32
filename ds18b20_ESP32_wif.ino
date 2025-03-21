#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

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

const char* ssid = "name";  
const char* password =  "passw";

WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_server = "server";
unsigned long lastmillis = 0;
int timeToSend = 10000; //10s
bool send_addres = false;


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Inicializando");

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
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi..");
  int restart = 0;
  while (WiFi.status() != WL_CONNECTED && restart < 30) {
    digitalWrite(LED_5, LOW);
    delay(150);
    digitalWrite(LED_5, HIGH);
    delay(150);
    Serial.print(".");
    restart++;
  }
  if (restart >= 30) {
    digitalWrite(LED_5, HIGH);
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(240);
    if (!wifiManager.autoConnect("Sensors_ds18b20", "ds18b20123")) {
      Serial.println(F("Falha na conexão. Resetar e tentar novamente..."));
      delay(500);
      ESP.restart();
    }
  }

  Serial.println(F("Conectado na rede Wifi."));
  Serial.print(F("Endereço IP: "));
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
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

void loop() {
  client.loop();
  if (!client.connected()) {
    digitalWrite(LED_5, LOW);
    Serial.println("CLIENTE DESCONECTADO!");
    reconnect();
  }

if(!send_addres){
  send_addres = true;
// Enviar lista de sensores detectados em cada barramento
sendDeviceList(sensors1, numberOfDevices1, "sensors_ds18b20/barramento_1");
sendDeviceList(sensors2, numberOfDevices2, "sensors_ds18b20/barramento_2");
sendDeviceList(sensors3, numberOfDevices3, "sensors_ds18b20/barramento_3");
sendDeviceList(sensors4, numberOfDevices4, "sensors_ds18b20/barramento_4");
}


  if (millis() - lastmillis > timeToSend) {
    lastmillis = millis();
    digitalWrite(LED_5, HIGH);
    sendTemperaturesWithAddress(sensors1, numberOfDevices1, "barramento_1", LED_1);
    sendTemperaturesWithAddress(sensors2, numberOfDevices2, "barramento_2", LED_2);
    sendTemperaturesWithAddress(sensors3, numberOfDevices3, "barramento_3", LED_3);
    sendTemperaturesWithAddress(sensors4, numberOfDevices4, "barramento_4", LED_4);
    digitalWrite(LED_5, LOW);
  }
}

void sendTemperaturesWithAddress(DallasTemperature& sensors, int numberOfDevices, const char* topic, int ledPin) {
  StaticJsonDocument<256> doc;
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
    timeToSend = PayLoad.toInt();
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
