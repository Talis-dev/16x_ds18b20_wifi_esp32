/*
 * http_client.ino
 * Envio de dados ao servidor externo via HTTP POST (JSON).
 */

extern Config cfg;

// -------------------------------------------------------
// POST genérico: envia body JSON para cfg.serverHost/path
// Retorna código HTTP (200, -1 se falha)
// -------------------------------------------------------
static int httpPost(const char* path, const String& body) {
  EthernetClient client;

  if (!client.connect(cfg.serverHost, cfg.serverPort)) {
    Serial.printf("[HTTP] Falha ao conectar em %s:%u\n", cfg.serverHost, cfg.serverPort);
    return -1;
  }

  // Monta requisição HTTP/1.1 manualmente (sem lib extra)
  client.printf("POST %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", cfg.serverHost);
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %u\r\n", body.length());
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // Aguarda resposta (timeout 5 s)
  unsigned long t = millis();
  while (!client.available() && millis() - t < 5000) delay(10);

  // Lê linha de status: "HTTP/1.1 200 OK"
  int statusCode = -1;
  if (client.available()) {
    String line = client.readStringUntil('\n');
    int spaceIdx = line.indexOf(' ');
    if (spaceIdx > 0) {
      statusCode = line.substring(spaceIdx + 1, spaceIdx + 4).toInt();
    }
  }

  client.stop();

  if (statusCode > 0) {
    Serial.printf("[HTTP] POST %s → %d\n", path, statusCode);
  } else {
    Serial.printf("[HTTP] POST %s → sem resposta válida\n", path);
  }
  return statusCode;
}

// -------------------------------------------------------
// Envia lista de endereços encontrados (chamado 1x no boot
// e após novo scan)
// -------------------------------------------------------
void httpPostAddresses() {
  String json = buildAddressJson();
  Serial.println(F("[HTTP] Enviando endereços..."));
  Serial.println(json);
  httpPost(cfg.serverPath, json);
}

// -------------------------------------------------------
// Envia leituras de temperatura + status do dispositivo
// -------------------------------------------------------
void httpPostReadings() {
  ethernetMaintain();                     // mantém DHCP/link

  // Leituras de sensores
  String readings = buildReadingsJson();

  // Status do dispositivo
  StaticJsonDocument<512> doc;
  deserializeJson(doc, readings);         // base
  doc["eth"]    = serialized(ethernetStatusJson());
  doc["free_heap"] = ESP.getFreeHeap();

  String finalJson;
  serializeJson(doc, finalJson);

  Serial.println(F("[HTTP] Enviando leituras..."));
  httpPost(cfg.serverPath, finalJson);
}
