/*
 * web_server.ino
 * Servidor HTTP embutido (porta 80).
 * HTML dinamico com dados embutidos - sem JavaScript/AJAX.
 * Auto-refresh via meta http-equiv refresh.
 *
 * Rotas:
 *   GET  /           -> Pagina HTML com sensores, status e config
 *   POST /config     -> Salva configuracoes (form HTML)
 *   POST /scan       -> Re-escaneia e redireciona para /
 *   GET  /api/status -> JSON (clientes externos)
 *   POST /api/config -> JSON config (clientes externos)
 *   POST /api/scan   -> JSON scan (clientes externos)
 */

extern Config         cfg;
extern EthernetServer webServer;
extern int            devCount1;
extern int            devCount2;
extern String         g_readingsCache;
extern DeviceAddress  addrBus1[16];
extern DeviceAddress  addrBus2[16];

// -------------------------------------------------------
// Helpers HTTP
// -------------------------------------------------------
static void sendOK(EthernetClient& c, const String& body,
                   const char* ctype = "application/json") {
  c.print(F("HTTP/1.1 200 OK\r\n"));
  c.printf("Content-Type: %s\r\n", ctype);
  c.printf("Content-Length: %u\r\n", (unsigned)body.length());
  c.print(F("Connection: close\r\n\r\n"));
  c.print(body);
}

static void sendRedirect(EthernetClient& c, const char* loc = "/") {
  c.printf("HTTP/1.1 303 See Other\r\nLocation: %s\r\nConnection: close\r\n\r\n", loc);
}

static void send400(EthernetClient& c) {
  c.print(F("HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
            "Connection: close\r\n\r\n{\"error\":\"bad request\"}"));
}

static void send404(EthernetClient& c) {
  c.print(F("HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n"
            "Connection: close\r\n\r\n{\"error\":\"not found\"}"));
}

// -------------------------------------------------------
// URL decode para formularios POST
// -------------------------------------------------------
static String urlDecode(const String& s) {
  String out;
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == '+') {
      out += ' ';
    } else if (s[i] == '%' && i + 2 < (int)s.length()) {
      char hex[3] = { s[i + 1], s[i + 2], '\0' };
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

static String formValue(const String& body, const char* key) {
  String search = String(key) + "=";
  int idx = body.indexOf(search);
  if (idx < 0) return "";
  idx += search.length();
  int end = body.indexOf('&', idx);
  if (end < 0) end = (int)body.length();
  return urlDecode(body.substring(idx, end));
}

// -------------------------------------------------------
// Imprime tabela de um barramento diretamente no client
// -------------------------------------------------------
static void printBusTable(EthernetClient& c, int busNum) {
  StaticJsonDocument<1024> cacheDoc;
  deserializeJson(cacheDoc, g_readingsCache);

  const char* key  = (busNum == 1) ? "bus1" : "bus2";
  int         gpio = (busNum == 1) ? 25 : 26;
  JsonArray   arr  = cacheDoc[key].as<JsonArray>();

  int total = arr.size();
  int ok    = 0;
  for (JsonObject s : arr) {
    if (!s.containsKey("error")) ok++;
  }
  int pct = (total > 0) ? (ok * 100 / total) : 0;

  const char* barColor = (pct >= 90) ? "#4caf50" : (pct >= 60 ? "#ff9800" : "#f44336");
  const char* note = (pct == 100) ? "Otimo" :
                     (pct >= 75)  ? "Bom" :
                     (pct >= 50)  ? "Regular - ajuste pull-up" : "Ruim - verifique cabos";

  c.printf("<div class='card'><h3>Barramento %d - GPIO %d</h3>", busNum, gpio);
  c.printf("<div style='font-size:.85em'>%d/%d respondendo - "
           "<strong style='color:%s'>%d%%</strong> - %s</div>",
           ok, total, barColor, pct, note);
  c.printf("<div style='background:#0f3460;border-radius:4px;height:8px;margin-top:4px'>"
           "<div style='width:%d%%;height:8px;border-radius:4px;background:%s'></div></div>",
           pct, barColor);

  c.print(F("<table style='margin-top:8px'><thead><tr>"
            "<th>Endereco</th><th>Temperatura</th><th>Status</th>"
            "</tr></thead><tbody>"));

  if (total == 0) {
    c.print(F("<tr><td colspan='3' style='color:#888'>Nenhum sensor</td></tr>"));
  } else {
    for (JsonObject s : arr) {
      const char* addr = s["addr"] | "?";
      if (s.containsKey("error")) {
        const char* err = s["error"] | "erro";
        c.printf("<tr>"
                 "<td style='font-family:monospace;font-size:.9em'>%s</td>"
                 "<td style='color:#f44336'>--</td>"
                 "<td style='color:#f44336'>%s</td>"
                 "</tr>", addr, err);
      } else {
        float t = s["temp"] | -127.0f;
        const char* cls = (t > 80) ? "#ff9800" : "#4caf50";
        c.printf("<tr>"
                 "<td style='font-family:monospace;font-size:.9em'>%s</td>"
                 "<td style='color:%s'>%.1f C</td>"
                 "<td style='color:#4caf50'>OK</td>"
                 "</tr>", addr, cls, t);
      }
    }
  }
  c.print(F("</tbody></table></div>"));
}

// -------------------------------------------------------
// GET / -- pagina HTML dinamica sem JavaScript
// -------------------------------------------------------
static void handleRoot(EthernetClient& c) {
  String ethJson = ethernetStatusJson();
  StaticJsonDocument<200> eth;
  deserializeJson(eth, ethJson);
  const char* ethIP   = eth["ip"]      | "?";
  const char* ethGW   = eth["gateway"] | "?";
  const char* ethLink = eth["link"]    | "?";
  bool        linkOK  = (strcmp(ethLink, "ON") == 0);

  unsigned long upSec    = millis() / 1000;
  unsigned int  heap     = (unsigned int)ESP.getFreeHeap();
  unsigned int  refresh  = (unsigned int)(cfg.pollInterval / 1000) + 3;

  // --- Linha de status + headers ---
  c.print(F("HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n"));

  // --- <head> ---
  c.print(F("<!DOCTYPE html><html lang='pt-BR'><head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  c.printf("<meta http-equiv='refresh' content='%u'>\n", refresh);
  c.print(F("<title>DS18B20 Monitor</title><style>"
            "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;"
            "margin:0;padding:16px}"
            "h1{color:#00d4ff;margin-bottom:2px}"
            ".sub{color:#888;font-size:.85em;margin-bottom:18px}"
            ".card{background:#16213e;border-radius:8px;padding:14px;margin-bottom:14px}"
            "h3{margin:0 0 8px;color:#00d4ff;font-size:1em}"
            "table{width:100%;border-collapse:collapse}"
            "th,td{padding:6px 10px;text-align:left;border-bottom:1px solid #0f3460}"
            "th{color:#aaa;font-weight:normal;font-size:.85em}"
            ".row{display:flex;gap:12px;flex-wrap:wrap}"
            ".row .card{flex:1;min-width:180px}"
            ".badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.8em}"
            ".bon{background:#1b5e20;color:#81c784}"
            ".boff{background:#b71c1c;color:#ef9a9a}"
            "input{background:#0f3460;color:#eee;border:1px solid #00d4ff;"
            "border-radius:4px;padding:6px;width:100%;box-sizing:border-box;margin-top:4px}"
            "button,input[type=submit]{background:#00d4ff;color:#000;border:none;"
            "border-radius:4px;padding:8px 16px;cursor:pointer;font-weight:bold;"
            "margin-top:8px}"
            ".bw{background:#ff9800}"
            "label{font-size:.85em;color:#aaa;display:block;margin-top:6px}"
            ".grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
            "</style></head><body>"));

  // --- Cabecalho ---
  c.print(F("<h1>DS18B20 Monitor</h1>"));
  c.printf("<div class='sub'>Dispositivo: %s | Auto-refresh: %us</div>",
           cfg.deviceName, refresh);

  // --- Cards de status ---
  c.print(F("<div class='row'>"));

  c.print(F("<div class='card'><h3>Rede</h3>"));
  c.printf("IP: %s<br>Gateway: %s<br>Link: "
           "<span class='badge %s'>%s</span></div>",
           ethIP, ethGW, linkOK ? "bon" : "boff", ethLink);

  c.printf("<div class='card'><h3>Uptime</h3>"
           "<span style='font-size:1.3em;color:#00d4ff'>"
           "%luh %02lum %02lus</span></div>",
           upSec / 3600, (upSec % 3600) / 60, upSec % 60);

  c.printf("<div class='card'><h3>Heap livre</h3>"
           "<span style='font-size:1.3em;color:#00d4ff'>%u bytes</span></div>",
           heap);

  c.print(F("</div>"));

  // --- Tabelas de barramentos ---
  printBusTable(c, 1);
  printBusTable(c, 2);

  // --- Formulario de configuracao ---
  c.print(F("<div class='card'><h3>Configuracoes</h3>"
            "<form method='POST' action='/config'>"
            "<div class='grid2'>"));

  c.printf("<div><label>Host do servidor</label>"
           "<input name='host' value='%s'></div>", cfg.serverHost);
  c.printf("<div><label>Porta</label>"
           "<input name='port' type='number' value='%u'></div>", cfg.serverPort);
  c.printf("<div><label>Caminho (path)</label>"
           "<input name='path' value='%s'></div>", cfg.serverPath);
  c.printf("<div><label>Intervalo polling (ms)</label>"
           "<input name='poll' type='number' value='%lu'></div>",
           cfg.pollInterval);
  c.printf("<div><label>Nome do dispositivo</label>"
           "<input name='name' value='%s'></div>", cfg.deviceName);

  c.print(F("</div>"
            "<input type='submit' value='Salvar'>"
            "</form>"
            "<form method='POST' action='/scan' "
            "style='display:inline-block;margin-left:8px'>"
            "<button class='bw' type='submit'>Re-escanear</button>"
            "</form></div>"));

  c.print(F("</body></html>"));
}

// -------------------------------------------------------
// POST /config  -- salva configuracao via form HTML
// -------------------------------------------------------
static void handleFormConfig(EthernetClient& c, const String& body) {
  String host = formValue(body, "host");
  String port = formValue(body, "port");
  String path = formValue(body, "path");
  String poll = formValue(body, "poll");
  String name = formValue(body, "name");

  if (host.length() > 0) strlcpy(cfg.serverHost, host.c_str(), sizeof(cfg.serverHost));
  if (port.length() > 0) cfg.serverPort   = (uint16_t)port.toInt();
  if (path.length() > 0) strlcpy(cfg.serverPath, path.c_str(), sizeof(cfg.serverPath));
  if (poll.length() > 0) cfg.pollInterval = (uint32_t)poll.toInt();
  if (name.length() > 0) strlcpy(cfg.deviceName, name.c_str(), sizeof(cfg.deviceName));

  cfg.magic = EEPROM_MAGIC;
  configSave(cfg);
  Serial.println(F("[WEB] Config salva via form"));
  sendRedirect(c, "/");
}

// -------------------------------------------------------
// POST /scan  -- escaneia e redireciona
// -------------------------------------------------------
static void handleFormScan(EthernetClient& c) {
  Serial.println(F("[WEB] Scan via form"));
  scanBuses();
  updateReadingsCache();
  httpPostAddresses();
  sendRedirect(c, "/");
}

// -------------------------------------------------------
// GET /api/status  (clientes externos / debug)
// -------------------------------------------------------
static void handleApiStatus(EthernetClient& c) {
  String out = g_readingsCache;
  if (out.endsWith("}")) {
    out.remove(out.length() - 1);
    out += F(",\"eth\":");
    out += ethernetStatusJson();
    out += F(",\"free_heap\":");
    out += ESP.getFreeHeap();
    out += F(",\"config\":{\"serverHost\":\"");
    out += cfg.serverHost;
    out += F("\",\"serverPort\":");
    out += cfg.serverPort;
    out += F(",\"serverPath\":\"");
    out += cfg.serverPath;
    out += F("\",\"pollInterval\":");
    out += cfg.pollInterval;
    out += F(",\"deviceName\":\"");
    out += cfg.deviceName;
    out += F("\"}}");
  }
  sendOK(c, out);
}

// -------------------------------------------------------
// POST /api/config  (clientes externos / JSON)
// -------------------------------------------------------
static void handleApiConfig(EthernetClient& c, const String& body) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body)) { send400(c); return; }

  if (doc.containsKey("serverHost"))
    strlcpy(cfg.serverHost, doc["serverHost"] | cfg.serverHost, sizeof(cfg.serverHost));
  if (doc.containsKey("serverPort"))
    cfg.serverPort = doc["serverPort"] | cfg.serverPort;
  if (doc.containsKey("serverPath"))
    strlcpy(cfg.serverPath, doc["serverPath"] | cfg.serverPath, sizeof(cfg.serverPath));
  if (doc.containsKey("pollInterval"))
    cfg.pollInterval = doc["pollInterval"] | cfg.pollInterval;
  if (doc.containsKey("deviceName"))
    strlcpy(cfg.deviceName, doc["deviceName"] | cfg.deviceName, sizeof(cfg.deviceName));

  cfg.magic = EEPROM_MAGIC;
  configSave(cfg);
  sendOK(c, F("{\"ok\":true}"));
}

// -------------------------------------------------------
// GET /api/diag  (clientes externos / JSON)
// -------------------------------------------------------
static void handleApiDiag(EthernetClient& c) {
  String d1 = busDiagJson(1);
  String d2 = busDiagJson(2);
  String out = "{\"bus1\":" + d1 + ",\"bus2\":" + d2 + "}";
  sendOK(c, out);
}

// -------------------------------------------------------
// POST /api/scan  (clientes externos / JSON)
// -------------------------------------------------------
static void handleApiScan(EthernetClient& c) {
  scanBuses();
  updateReadingsCache();
  httpPostAddresses();

  StaticJsonDocument<768> doc;
  doc["ok"] = true;
  char addrStr[17];

  JsonArray b1 = doc.createNestedArray("bus1");
  for (int i = 0; i < devCount1; i++) {
    for (uint8_t b = 0; b < 8; b++) sprintf(addrStr + b * 2, "%02X", addrBus1[i][b]);
    addrStr[16] = '\0';
    b1.add(addrStr);
  }

  JsonArray b2 = doc.createNestedArray("bus2");
  for (int i = 0; i < devCount2; i++) {
    for (uint8_t b = 0; b < 8; b++) sprintf(addrStr + b * 2, "%02X", addrBus2[i][b]);
    addrStr[16] = '\0';
    b2.add(addrStr);
  }

  String out;
  serializeJson(doc, out);
  sendOK(c, out);
}

// -------------------------------------------------------
// Despacha requisicao recebida
// -------------------------------------------------------
static void dispatchRequest(EthernetClient& c, const String& method,
                            const String& path, const String& body) {
  Serial.printf("[WEB] %s %s\n", method.c_str(), path.c_str());

  if (path == "/" && method == "GET") {
    handleRoot(c);
  } else if (path == "/config" && method == "POST") {
    handleFormConfig(c, body);
  } else if (path == "/scan" && method == "POST") {
    handleFormScan(c);
  } else if (path == "/api/status" && method == "GET") {
    handleApiStatus(c);
  } else if (path == "/api/diag" && method == "GET") {
    handleApiDiag(c);
  } else if (path == "/api/config" && method == "POST") {
    handleApiConfig(c, body);
  } else if (path == "/api/scan" && method == "POST") {
    handleApiScan(c);
  } else {
    send404(c);
  }
}

// -------------------------------------------------------
// Le body de requisicao POST (ate 2 KB)
// -------------------------------------------------------
static String readBody(EthernetClient& c, int contentLength) {
  String body = "";
  if (contentLength <= 0 || contentLength > 2048) return body;
  body.reserve(contentLength);
  unsigned long t = millis();
  while ((int)body.length() < contentLength && millis() - t < 3000) {
    if (c.available()) {
      body += (char)c.read();
    } else {
      delay(1);
    }
  }
  return body;
}

// -------------------------------------------------------
// Chamado no loop() principal
// -------------------------------------------------------
void handleWebClient() {
  EthernetClient client = webServer.available();
  if (!client) return;

  client.setTimeout(500);

  unsigned long t = millis();
  String reqLine      = "";
  int    contentLength = 0;

  while (client.connected() && millis() - t < 2000) {
    if (!client.available()) { delay(1); continue; }
    String line = client.readStringUntil('\n');
    line.trim();
    if (reqLine.isEmpty()) {
      reqLine = line;
    } else if (line.isEmpty()) {
      break;
    } else {
      String lower = line;
      lower.toLowerCase();
      if (lower.startsWith("content-length:")) {
        String val = lower.substring(15);
        val.trim();
        contentLength = val.toInt();
      }
    }
  }

  if (reqLine.isEmpty()) { client.stop(); return; }

  int s1 = reqLine.indexOf(' ');
  int s2 = reqLine.indexOf(' ', s1 + 1);
  if (s1 < 0 || s2 < 0) { client.stop(); return; }

  String method = reqLine.substring(0, s1);
  String path   = reqLine.substring(s1 + 1, s2);

  String body = (method == "POST" && contentLength > 0)
                ? readBody(client, contentLength)
                : "";

  dispatchRequest(client, method, path, body);

  client.flush();
  delay(20);
  client.stop();
}
