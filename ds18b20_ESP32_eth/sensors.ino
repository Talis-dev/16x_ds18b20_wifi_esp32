/*
 * sensors.ino
 * Varredura e leitura dos 2 barramentos OneWire DS18B20.
 * Inclui rotina de medição de qualidade do barramento.
 */

extern DallasTemperature bus1;
extern DallasTemperature bus2;
extern int devCount1;
extern int devCount2;
extern DeviceAddress addrBus1[16];
extern DeviceAddress addrBus2[16];

// -------------------------------------------------------
// Cache de leituras (atualizado pelo loop de polling)
// -------------------------------------------------------
String g_readingsCache = "{}";

void updateReadingsCache() {
  g_readingsCache = buildReadingsJson();
}

// -------------------------------------------------------
// Helpers
// -------------------------------------------------------
static void addrToStr(const DeviceAddress addr, char* out) {
  for (uint8_t i = 0; i < 8; i++) {
    sprintf(out + i * 2, "%02X", addr[i]);
  }
  out[16] = '\0';
}

// -------------------------------------------------------
// Varre os dois barramentos e armazena endereços
// -------------------------------------------------------
void scanBuses() {
  Serial.println(F("[SNS] Varrendo barramentos..."));

  bus1.begin();
  bus2.begin();

  devCount1 = bus1.getDeviceCount();
  devCount2 = bus2.getDeviceCount();

  for (int i = 0; i < devCount1 && i < 16; i++) {
    bus1.getAddress(addrBus1[i], i);
  }
  for (int i = 0; i < devCount2 && i < 16; i++) {
    bus2.getAddress(addrBus2[i], i);
  }

  Serial.printf("[SNS] Bus1: %d sensor(es)  Bus2: %d sensor(es)\n", devCount1, devCount2);
}

// -------------------------------------------------------
// Retorna JSON com endereços detectados em ambos os barramentos
// {
//   "device": "...",
//   "bus1": ["AABBCCDDEEFF0011", ...],
//   "bus2": [...]
// }
// -------------------------------------------------------
String buildAddressJson() {
  StaticJsonDocument<1024> doc;
  doc["device"] = cfg.deviceName;

  JsonArray b1 = doc.createNestedArray("bus1");
  for (int i = 0; i < devCount1; i++) {
    char s[17]; addrToStr(addrBus1[i], s);
    b1.add(s);
  }

  JsonArray b2 = doc.createNestedArray("bus2");
  for (int i = 0; i < devCount2; i++) {
    char s[17]; addrToStr(addrBus2[i], s);
    b2.add(s);
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// -------------------------------------------------------
// Lê temperaturas de um barramento e acende LED de atividade.
// Retorna JSON array: [{"addr":"...","temp":25.5}, ...]
// -------------------------------------------------------
static String readBusJson(DallasTemperature& bus, int count, DeviceAddress addrs[], int ledPin) {
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.to<JsonArray>();

  ledSet(ledPin, LOW);   // LOW = aceso (active-low)
  bus.requestTemperatures();

  for (int i = 0; i < count; i++) {
    float t = bus.getTempC(addrs[i]);
    char s[17]; addrToStr(addrs[i], s);

    JsonObject obj = arr.createNestedObject();
    obj["addr"] = s;
    if (t == DEVICE_DISCONNECTED_C) {
      obj["error"] = "disconnected";
    } else {
      obj["temp"] = round(t * 10.0f) / 10.0f;   // 1 casa decimal
    }
  }

  ledSet(ledPin, HIGH);  // HIGH = apagado (active-low)

  String out;
  serializeJson(doc, out);
  return out;
}

// -------------------------------------------------------
// Constrói JSON completo de leituras para HTTP POST
// {
//   "device": "...",
//   "uptime": 12345,
//   "bus1": [...],
//   "bus2": [...]
// }
// -------------------------------------------------------
String buildReadingsJson() {
  // Lê os dois barramentos (os LEDs piscam durante a leitura)
  String b1 = readBusJson(bus1, devCount1, addrBus1, LED_BUS1);
  String b2 = readBusJson(bus2, devCount2, addrBus2, LED_BUS2);

  StaticJsonDocument<3072> doc;
  doc["device"]  = cfg.deviceName;
  doc["uptime"]  = millis() / 1000;

  // Deserializa os arrays parciais e encaixa no doc principal
  StaticJsonDocument<1024> tmpDoc;
  deserializeJson(tmpDoc, b1);
  doc["bus1"] = tmpDoc.as<JsonArray>();
  deserializeJson(tmpDoc, b2);
  doc["bus2"] = tmpDoc.as<JsonArray>();

  String out;
  serializeJson(doc, out);
  return out;
}

// -------------------------------------------------------
// Mede a qualidade do barramento OneWire:
// Conta quantos sensores respondem vs. quantos foram descobertos,
// e testa if a temperatura está fora da faixa de erro.
// Retorna objeto JSON:
// {
//   "bus": 1,
//   "discovered": 4,
//   "responding": 3,
//   "quality_pct": 75,
//   "note": "..."
// }
// -------------------------------------------------------
String busDiagJson(int busNum) {
  DallasTemperature& bus    = (busNum == 1) ? bus1 : bus2;
  int                count  = (busNum == 1) ? devCount1 : devCount2;
  DeviceAddress*     addrs  = (busNum == 1) ? addrBus1  : addrBus2;
  int                ledPin = (busNum == 1) ? LED_BUS1  : LED_BUS2;

  ledSet(ledPin, LOW);   // LOW = aceso (active-low)
  bus.requestTemperatures();
  int responding = 0;
  for (int i = 0; i < count; i++) {
    float t = bus.getTempC(addrs[i]);
    if (t != DEVICE_DISCONNECTED_C) responding++;
  }
  ledSet(ledPin, HIGH);  // HIGH = apagado (active-low)

  int quality = (count > 0) ? (responding * 100 / count) : 0;

  StaticJsonDocument<200> doc;
  doc["bus"]          = busNum;
  doc["discovered"]   = count;
  doc["responding"]   = responding;
  doc["quality_pct"]  = quality;
  if      (quality == 100) doc["note"] = "Otimo";
  else if (quality >= 75)  doc["note"] = "Bom";
  else if (quality >= 50)  doc["note"] = "Regular – ajuste o pull-up";
  else                     doc["note"] = "Ruim – verifique cabos e pull-up";

  String out;
  serializeJson(doc, out);
  return out;
}
