/*
 * web_server.ino
 * Servidor HTTP embarcado (porta 80) para gerenciamento MQTT e visualizacao de sensores.
 *
 * Rotas HTML (formulario):
 *   GET  /                   -> Pagina: status, barramentos, controles, config MQTT
 *   POST /config             -> Salva configuracoes MQTT (form)
 *   POST /scan               -> Re-escaneia sensores OneWire
 *
 * API REST (JSON):
 *   GET  /api/status         -> Status completo do dispositivo
 *   GET  /api/config         -> Configuracao atual (sem senha)
 *   POST /api/config         -> Atualiza configuracao via JSON body
 *   POST /api/restart        -> Solicita reinicio do ESP32
 *   POST /api/mqtt/reconnect -> Forca reconexao MQTT imediata
 */

extern bool          mqttFailed;
extern int           reboot;
extern bool          mqttEverConnected;
extern unsigned long g_mqttRetryInterval;
extern bool          restartRequested;
extern unsigned long restartAt;
extern bool          send_addres;
extern void          mqttForceReconnect();

// -------------------------------------------------------
// Envia tabela HTML de um barramento
// -------------------------------------------------------
static void sendBusTable(int busNum) {
  static const int gpios[] = { 23, 19, 18, 5 };

  StaticJsonDocument<1024> doc;
  deserializeJson(doc, g_busCache[busNum - 1]);
  JsonArray arr = doc.as<JsonArray>();

  int total = arr.size();
  int ok    = 0;
  for (JsonObject s : arr) {
    if (!s.containsKey("error")) ok++;
  }
  int pct = (total > 0) ? (ok * 100 / total) : 0;

  const char* barColor = (pct >= 90) ? "#4caf50" : (pct >= 60 ? "#ff9800" : "#f44336");
  const char* note = (total == 0) ? "Sem dados - aguardando leitura" :
                     (pct == 100) ? "Otimo"   :
                     (pct >= 75)  ? "Bom"     :
                     (pct >= 50)  ? "Regular" : "Ruim - verifique cabos";

  String html = "<div class='card'>";
  html += "<h3>Barramento "; html += busNum;
  html += " &mdash; GPIO "; html += gpios[busNum - 1]; html += "</h3>";
  html += "<div class='bus-stat'>";
  html += ok; html += "/"; html += total;
  html += " respondendo &nbsp;<strong style='color:"; html += barColor; html += "'>";
  html += pct; html += "%</strong> &mdash; "; html += note; html += "</div>";
  html += "<div class='prog-bg'><div class='prog-fill' style='width:";
  html += pct; html += "%;background:"; html += barColor; html += "'></div></div>";
  webServer.sendContent(html);

  webServer.sendContent(F(
    "<table><thead><tr>"
    "<th>Endereco</th><th>Temperatura</th><th>Status</th>"
    "</tr></thead><tbody>"));

  if (total == 0) {
    webServer.sendContent(F("<tr><td colspan='3' class='empty'>Nenhum dado ainda</td></tr>"));
  } else {
    char buf[128];
    for (JsonObject s : arr) {
      const char* addr = s["addr"] | "?";
      if (s.containsKey("error")) {
        const char* err = s["error"] | "erro";
        snprintf(buf, sizeof(buf),
          "<tr><td class='mono'>%s</td>"
          "<td class='err'>--</td><td class='err'>%s</td></tr>", addr, err);
      } else {
        float t = s["temp"] | -127.0f;
        const char* cls = (t > 80) ? "warn" : "ok";
        snprintf(buf, sizeof(buf),
          "<tr><td class='mono'>%s</td>"
          "<td class='%s'>%.1f &deg;C</td><td class='ok'>OK</td></tr>", addr, cls, t);
      }
      webServer.sendContent(buf);
    }
  }
  webServer.sendContent(F("</tbody></table></div>"));
}

// -------------------------------------------------------
// GET /  -- pagina principal
// -------------------------------------------------------
static void handleRoot() {
  unsigned long upSec = millis() / 1000;
  unsigned int  heap  = (unsigned int)ESP.getFreeHeap();
  unsigned int  refreshSec = (unsigned int)(cfg.pollInterval / 1000) + 3;

  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, F("text/html; charset=UTF-8"), "");

  // HEAD + CSS
  webServer.sendContent(F(
    "<!DOCTYPE html><html lang='pt-BR'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>DS18B20 WiFi Monitor</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:16px}"
    "h1{color:#00d4ff;margin:0 0 4px}"
    ".sub{color:#888;font-size:.85em;margin-bottom:16px}"
    ".card{background:#16213e;border-radius:8px;padding:14px;margin-bottom:12px}"
    "h3{margin:0 0 10px;color:#00d4ff;font-size:1em}"
    ".srow{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:12px}"
    ".srow .card{flex:1;min-width:160px;margin-bottom:0}"
    ".badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.82em;font-weight:bold}"
    ".bon{background:#1b5e20;color:#81c784}"
    ".boff{background:#b71c1c;color:#ef9a9a}"
    ".bwarn{background:#e65100;color:#ffcc80}"
    "table{width:100%;border-collapse:collapse;margin-top:8px}"
    "th,td{padding:6px 10px;text-align:left;border-bottom:1px solid #0f3460}"
    "th{color:#aaa;font-weight:normal;font-size:.83em}"
    ".mono{font-family:monospace;font-size:.9em}"
    ".ok{color:#4caf50}"
    ".warn{color:#ff9800}"
    ".err{color:#f44336}"
    ".empty{color:#666}"
    ".bus-stat{font-size:.85em;margin-bottom:4px}"
    ".prog-bg{background:#0f3460;border-radius:4px;height:7px;margin-bottom:6px}"
    ".prog-fill{height:7px;border-radius:4px}"
    "label{font-size:.85em;color:#aaa;display:block;margin-top:8px}"
    "input[type=text],input[type=number],input[type=password]{"
    "background:#0f3460;color:#eee;border:1px solid #00d4ff;"
    "border-radius:4px;padding:7px;width:100%;box-sizing:border-box;margin-top:3px}"
    "input[type=submit],button{"
    "background:#00d4ff;color:#000;border:none;border-radius:4px;"
    "padding:9px 18px;cursor:pointer;font-weight:bold;margin-top:10px}"
    ".btn-warn{background:#ff9800}"
    ".btn-danger{background:#f44336;color:#fff}"
    ".grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
    "#refresh-bar{font-size:.78em;color:#555;margin-bottom:10px}"
    "</style></head><body>"));

  // JS: auto-refresh que pausa quando usuario esta editando
  webServer.sendContent(F("<script>var t="));
  webServer.sendContent(String(refreshSec));
  webServer.sendContent(F(",editing=false;"
    "document.addEventListener('focusin',function(e){"
    "if(e.target.tagName==='INPUT')editing=true;});"
    "document.addEventListener('focusout',function(e){"
    "if(e.target.tagName==='INPUT')editing=false;});"
    "setInterval(function(){"
    "if(editing)return;"
    "t--;var el=document.getElementById('rc');"
    "if(el)el.textContent=t+'s';"
    "if(t<=0)location.reload();"
    "},1000);"
    "function restartCpu(){"
    "if(!confirm('Reiniciar o ESP32?'))return;"
    "var m=document.getElementById('ctrl-msg');"
    "if(m){m.textContent='Reiniciando... aguarde 5s';m.style.color='#f44336';}"
    "fetch('/api/restart',{method:'POST'})"
    ".then(function(){setTimeout(function(){location.reload();},5000);});"
    "}"
    "function mqttReconnect(){"
    "var m=document.getElementById('ctrl-msg');"
    "if(m){m.textContent='Reconectando...';m.style.color='#ff9800';}"
    "fetch('/api/mqtt/reconnect',{method:'POST'})"
    ".then(function(r){return r.json();})"
    ".then(function(d){"
    "if(m){m.textContent=d.message||'OK';m.style.color='#4caf50';}"
    "setTimeout(function(){location.reload();},2000);});"
    "}"
    "</script>"));

  // Cabecalho
  {
    String hdr = "<h1>DS18B20 WiFi Monitor</h1><div class='sub'>Dispositivo: <strong>";
    hdr += cfg.deviceName;
    hdr += "</strong> &nbsp;|&nbsp; IP: <strong>";
    hdr += WiFi.localIP().toString();
    hdr += "</strong></div><div id='refresh-bar'>Auto-refresh em <span id='rc'>";
    hdr += refreshSec;
    hdr += "s</span> (pausa ao editar)</div>";
    webServer.sendContent(hdr);
  }

  // Cards de status
  webServer.sendContent(F("<div class='srow'>"));

  // WiFi
  {
    int rssi = WiFi.RSSI();
    const char* rc = (rssi > -60) ? "#4caf50" : (rssi > -75 ? "#ff9800" : "#f44336");
    String c = "<div class='card'><h3>WiFi</h3>SSID: ";
    c += WiFi.SSID();
    c += "<br>RSSI: <span style='color:"; c += rc; c += "'>";
    c += rssi; c += " dBm</span></div>";
    webServer.sendContent(c);
  }

  // MQTT
  {
    bool mqttOk = client.connected();
    const char* lbl = mqttOk     ? "Conectado"               :
                      mqttFailed ? "Erro - verifique config"  :
                                   "Reconectando...";
    const char* badge = mqttOk ? "bon" : (mqttFailed ? "boff" : "bwarn");
    String c = "<div class='card'><h3>MQTT</h3>";
    c += cfg.mqttServer; c += ":"; c += cfg.mqttPort;
    c += "<br><span class='badge "; c += badge; c += "'>"; c += lbl; c += "</span></div>";
    webServer.sendContent(c);
  }

  // Uptime
  {
    char c[128];
    snprintf(c, sizeof(c),
      "<div class='card'><h3>Uptime</h3>"
      "<span style='color:#00d4ff;font-size:1.2em'>%luh %02lum %02lus</span></div>",
      upSec / 3600, (upSec % 3600) / 60, upSec % 60);
    webServer.sendContent(c);
  }

  // Heap
  {
    char c[128];
    snprintf(c, sizeof(c),
      "<div class='card'><h3>Heap livre</h3>"
      "<span style='color:#00d4ff;font-size:1.2em'>%u B</span></div>", heap);
    webServer.sendContent(c);
  }

  webServer.sendContent(F("</div>")); // fim .srow

  // Card de controles
  webServer.sendContent(F(
    "<div class='card'><h3>Controles</h3>"
    "<span id='ctrl-msg' style='font-size:.85em;display:block;margin-bottom:8px'></span>"
    "<button class='btn-warn' onclick='mqttReconnect()'>Reconectar MQTT</button>"
    "&nbsp;"
    "<button class='btn-danger' onclick='restartCpu()'>Reiniciar CPU</button>"
    "</div>"));

  // Tabelas dos 4 barramentos
  sendBusTable(1);
  sendBusTable(2);
  sendBusTable(3);
  sendBusTable(4);

  // Formulario MQTT
  webServer.sendContent(F("<div class='card'><h3>Configuracoes MQTT</h3>"
    "<form method='POST' action='/config'><div class='grid2'>"));

  {
    String f = "";
    f += "<div><label>Servidor MQTT</label><input type='text' name='server' value='"; f += cfg.mqttServer; f += "'></div>";
    f += "<div><label>Porta</label><input type='number' name='port' value='"; f += cfg.mqttPort; f += "'></div>";
    f += "<div><label>Client ID</label><input type='text' name='clientid' value='"; f += cfg.mqttClientId; f += "'></div>";
    f += "<div><label>Usuario</label><input type='text' name='user' value='"; f += cfg.mqttUser; f += "'></div>";
    f += "<div><label>Senha</label><input type='password' name='pass' placeholder='(vazio = nao alterar)'></div>";
    f += "<div><label>Nome do dispositivo</label><input type='text' name='name' value='"; f += cfg.deviceName; f += "'></div>";
    f += "<div><label>Intervalo polling (ms)</label><input type='number' name='poll' value='"; f += cfg.pollInterval; f += "'></div>";
    webServer.sendContent(f);
  }

  webServer.sendContent(F("</div>"
    "<input type='submit' value='Salvar configuracoes'>"
    "</form>"
    "<form method='POST' action='/scan' style='display:inline-block;margin-left:10px'>"
    "<button type='submit' class='btn-warn'>Re-escanear sensores</button>"
    "</form></div></body></html>"));

  webServer.sendContent(""); // encerra chunked
}

// -------------------------------------------------------
// POST /config  -- salva via webServer.arg() (metodo correto para ESP32 WebServer)
// -------------------------------------------------------
static void handleFormConfig() {
  String server   = webServer.arg("server");
  String port     = webServer.arg("port");
  String clientid = webServer.arg("clientid");
  String user     = webServer.arg("user");
  String pass     = webServer.arg("pass");
  String name     = webServer.arg("name");
  String poll     = webServer.arg("poll");

  if (server.length()   > 0) strlcpy(cfg.mqttServer,   server.c_str(),   sizeof(cfg.mqttServer));
  if (port.length()     > 0) cfg.mqttPort     = (uint16_t)port.toInt();
  if (clientid.length() > 0) strlcpy(cfg.mqttClientId, clientid.c_str(), sizeof(cfg.mqttClientId));
  // usuario: permite apagar (campo vazio = sem autenticacao)
  strlcpy(cfg.mqttUser, user.c_str(), sizeof(cfg.mqttUser));
  if (pass.length()     > 0) strlcpy(cfg.mqttPass, pass.c_str(), sizeof(cfg.mqttPass));
  if (name.length()     > 0) strlcpy(cfg.deviceName, name.c_str(), sizeof(cfg.deviceName));
  if (poll.length()     > 0 && poll.toInt() >= 1000) cfg.pollInterval = (uint32_t)poll.toInt();

  configSave(cfg);

  mqttFailed = false;
  reboot     = 0;
  client.disconnect();
  client.setServer(cfg.mqttServer, cfg.mqttPort);

  Serial.printf("[WEB] Config salva: %s:%u  user='%s'\n",
    cfg.mqttServer, cfg.mqttPort, cfg.mqttUser);

  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// -------------------------------------------------------
// POST /scan
// -------------------------------------------------------
static void handleFormScan() {
  numberOfDevices1 = sensors1.getDeviceCount();
  numberOfDevices2 = sensors2.getDeviceCount();
  numberOfDevices3 = sensors3.getDeviceCount();
  numberOfDevices4 = sensors4.getDeviceCount();
  Serial.printf("[WEB] Re-scan: B1=%d B2=%d B3=%d B4=%d\n",
    numberOfDevices1, numberOfDevices2, numberOfDevices3, numberOfDevices4);
  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// -------------------------------------------------------
// GET /api/status
// -------------------------------------------------------
static void handleApiStatus() {
  StaticJsonDocument<512> doc;
  doc["ip"]                   = WiFi.localIP().toString();
  doc["rssi"]                 = WiFi.RSSI();
  doc["ssid"]                 = WiFi.SSID();
  doc["mqtt"]                 = client.connected();
  doc["mqttFail"]             = mqttFailed;
  doc["mqttRetries"]          = reboot;
  doc["mqttRetryIntervalMs"]  = g_mqttRetryInterval;
  doc["uptime_ms"]            = millis();
  doc["free_heap"]            = ESP.getFreeHeap();
  doc["poll_ms"]              = cfg.pollInterval;
  doc["device"]               = cfg.deviceName;
  doc["server"]               = cfg.mqttServer;
  doc["port"]                 = cfg.mqttPort;
  JsonObject devs             = doc.createNestedObject("devices");
  devs["bus1"] = numberOfDevices1;
  devs["bus2"] = numberOfDevices2;
  devs["bus3"] = numberOfDevices3;
  devs["bus4"] = numberOfDevices4;
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

// -------------------------------------------------------
// POST /api/restart
// -------------------------------------------------------
static void handleApiRestart() {
  webServer.send(200, "application/json",
    "{\"result\":\"ok\",\"message\":\"Reiniciando em 1.5s\"}");
  restartRequested = true;
  restartAt = millis() + 1500;
  Serial.println(F("[WEB] Reinicio solicitado via HTTP"));
}

// -------------------------------------------------------
// POST /api/mqtt/reconnect
// -------------------------------------------------------
static void handleApiMqttReconnect() {
  send_addres = false;     // reenvia lista de sensores apos reconexao
  mqttForceReconnect();
  webServer.send(200, "application/json",
    "{\"result\":\"ok\",\"message\":\"Reconexao MQTT iniciada\"}");
}

// -------------------------------------------------------
// GET /api/config  -- retorna config atual em JSON (sem senha)
// -------------------------------------------------------
static void handleApiConfigGet() {
  StaticJsonDocument<256> doc;
  doc["server"]   = cfg.mqttServer;
  doc["port"]     = cfg.mqttPort;
  doc["clientid"] = cfg.mqttClientId;
  doc["user"]     = cfg.mqttUser;
  doc["name"]     = cfg.deviceName;
  doc["poll_ms"]  = cfg.pollInterval;
  // senha nao retornada por seguranca
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

// -------------------------------------------------------
// POST /api/config  -- atualiza config via JSON body
// -------------------------------------------------------
static void handleApiConfigPostJson() {
  String body = webServer.arg("plain");
  if (body.length() == 0) {
    webServer.send(400, "application/json", "{\"error\":\"body vazio\"}");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    webServer.send(400, "application/json", "{\"error\":\"JSON invalido\"}");
    return;
  }
  if (doc.containsKey("server"))   strlcpy(cfg.mqttServer,   doc["server"]   | cfg.mqttServer,   sizeof(cfg.mqttServer));
  if (doc.containsKey("port"))     cfg.mqttPort     = (uint16_t)(doc["port"]   | (int)cfg.mqttPort);
  if (doc.containsKey("clientid")) strlcpy(cfg.mqttClientId, doc["clientid"] | cfg.mqttClientId, sizeof(cfg.mqttClientId));
  if (doc.containsKey("user"))     strlcpy(cfg.mqttUser,     doc["user"]     | cfg.mqttUser,     sizeof(cfg.mqttUser));
  if (doc.containsKey("pass"))     strlcpy(cfg.mqttPass,     doc["pass"]     | cfg.mqttPass,     sizeof(cfg.mqttPass));
  if (doc.containsKey("name"))     strlcpy(cfg.deviceName,   doc["name"]     | cfg.deviceName,   sizeof(cfg.deviceName));
  if (doc.containsKey("poll_ms")) {
    uint32_t p = (uint32_t)(doc["poll_ms"] | (int)cfg.pollInterval);
    if (p >= 1000) cfg.pollInterval = p;
  }
  configSave(cfg);
  mqttFailed = false;
  reboot = 0;
  client.disconnect();
  client.setServer(cfg.mqttServer, cfg.mqttPort);
  Serial.printf("[WEB] Config API salva: %s:%u\n", cfg.mqttServer, cfg.mqttPort);
  webServer.send(200, "application/json", "{\"result\":\"ok\",\"message\":\"Config salva\"}");
}

// -------------------------------------------------------
// Setup e loop
// -------------------------------------------------------
void webServerSetup() {
  webServer.on("/",                   HTTP_GET,  handleRoot);
  webServer.on("/config",             HTTP_POST, handleFormConfig);
  webServer.on("/scan",               HTTP_POST, handleFormScan);
  webServer.on("/api/status",         HTTP_GET,  handleApiStatus);
  webServer.on("/api/restart",        HTTP_POST, handleApiRestart);
  webServer.on("/api/mqtt/reconnect", HTTP_POST, handleApiMqttReconnect);
  webServer.on("/api/config",         HTTP_GET,  handleApiConfigGet);
  webServer.on("/api/config",         HTTP_POST, handleApiConfigPostJson);
  webServer.onNotFound([]() {
    webServer.send(404, "application/json", "{\"error\":\"not found\"}");
  });
  webServer.begin();
  Serial.println(F("[WEB] Servidor HTTP iniciado na porta 80"));
}

void handleWebClient() {
  webServer.handleClient();
}
