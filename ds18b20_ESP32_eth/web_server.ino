/*
 * web_server.ino
 * Servidor HTTP embutido (porta 80) – página de monitoramento
 * e API REST para configuração.
 *
 * Rotas:
 *   GET  /           → Página HTML com sensores, status e config
 *   GET  /api/status → JSON com últimas leituras + info de rede
 *   GET  /api/diag   → JSON diagnóstico de qualidade dos barramentos
 *   POST /api/config → Salva configurações (JSON body)
 *   POST /api/scan   → Re-escaneia barramentos e envia endereços
 */

extern Config          cfg;
extern EthernetServer  webServer;
extern int             devCount1;
extern int             devCount2;

// Cache das últimas leituras para a página web
static String lastReadingsJson = "{}";

// -------------------------------------------------------
// Atualiza cache (chamado após cada coleta real)
// -------------------------------------------------------
void webCacheReadings(const String& json) {
  lastReadingsJson = json;
}

// -------------------------------------------------------
// Handlers individuais
// -------------------------------------------------------

static void sendOK(EthernetClient& c, const String& body,
                   const char* ctype = "application/json") {
  c.print(F("HTTP/1.1 200 OK\r\n"));
  c.printf("Content-Type: %s\r\n", ctype);
  c.printf("Content-Length: %u\r\n", body.length());
  c.print(F("Connection: close\r\n\r\n"));
  c.print(body);
}

static void send400(EthernetClient& c) {
  const char* msg = "{\"error\":\"bad request\"}";
  c.print(F("HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"));
  c.print(msg);
}

static void send404(EthernetClient& c) {
  const char* msg = "{\"error\":\"not found\"}";
  c.print(F("HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"));
  c.print(msg);
}

// -------------------------------------------------------
// Página HTML principal
// -------------------------------------------------------
static void handleRoot(EthernetClient& c) {
  // Página minimalista, usa fetch() para atualizar dados
  String html = F(R"rawhtml(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DS18B20 Monitor</title>
<style>
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:16px}
  h1{color:#00d4ff;margin-bottom:4px}
  .subtitle{color:#888;font-size:.85em;margin-bottom:20px}
  .card{background:#16213e;border-radius:8px;padding:14px;margin-bottom:14px}
  .card h3{margin:0 0 10px;color:#00d4ff;font-size:1em}
  table{width:100%;border-collapse:collapse}
  th,td{padding:6px 10px;text-align:left;border-bottom:1px solid #0f3460}
  th{color:#aaa;font-weight:normal;font-size:.85em}
  .ok{color:#4caf50}.warn{color:#ff9800}.err{color:#f44336}
  .badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.8em}
  .badge-on{background:#1b5e20;color:#81c784}
  .badge-off{background:#b71c1c;color:#ef9a9a}
  input,select{background:#0f3460;color:#eee;border:1px solid #00d4ff;
    border-radius:4px;padding:6px;width:100%;box-sizing:border-box;margin-top:4px}
  button{background:#00d4ff;color:#000;border:none;border-radius:4px;
    padding:8px 18px;cursor:pointer;margin-top:10px;font-weight:bold}
  button:hover{background:#0099bb}
  .btn-warn{background:#ff9800;color:#000}
  .btn-warn:hover{background:#e65100}
  label{font-size:.85em;color:#aaa}
  .row{display:flex;gap:14px;flex-wrap:wrap}
  .row .card{flex:1;min-width:200px}
  #msg{color:#4caf50;min-height:20px;margin-top:8px;font-size:.9em}
  .quality-bar-bg{background:#0f3460;border-radius:4px;height:10px;margin-top:4px}
  .quality-bar{height:10px;border-radius:4px;transition:width .5s}
</style>
</head>
<body>
<h1>&#127777; DS18B20 Monitor</h1>
<div class="subtitle" id="devname">Carregando...</div>

<div class="row">
  <div class="card" id="card-net">
    <h3>&#127760; Rede</h3>
    <div id="net-info">...</div>
  </div>
  <div class="card">
    <h3>&#9203; Uptime</h3>
    <div id="uptime-val" style="font-size:1.4em;color:#00d4ff">--</div>
  </div>
  <div class="card">
    <h3>&#128150; Heap livre</h3>
    <div id="heap-val" style="font-size:1.4em;color:#00d4ff">--</div>
  </div>
</div>

<div class="card">
  <h3>&#127809; Barramento 1 – GPIO 25</h3>
  <div id="diag1" style="font-size:.85em;margin-bottom:8px"></div>
  <table><thead><tr><th>Endereço</th><th>Temperatura</th><th>Status</th></tr></thead>
  <tbody id="bus1-body"><tr><td colspan="3">Aguardando...</td></tr></tbody></table>
</div>

<div class="card">
  <h3>&#127809; Barramento 2 – GPIO 26</h3>
  <div id="diag2" style="font-size:.85em;margin-bottom:8px"></div>
  <table><thead><tr><th>Endereço</th><th>Temperatura</th><th>Status</th></tr></thead>
  <tbody id="bus2-body"><tr><td colspan="3">Aguardando...</td></tr></tbody></table>
</div>

<div class="card">
  <h3>&#9881;&#65039; Configurações</h3>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
    <div>
      <label>Host do servidor</label>
      <input id="cfg-host" placeholder="192.168.1.100">
    </div>
    <div>
      <label>Porta</label>
      <input id="cfg-port" type="number" placeholder="3000">
    </div>
    <div>
      <label>Caminho (path)</label>
      <input id="cfg-path" placeholder="/api/sensors">
    </div>
    <div>
      <label>Intervalo de polling (ms)</label>
      <input id="cfg-poll" type="number" placeholder="10000">
    </div>
    <div>
      <label>Nome do dispositivo</label>
      <input id="cfg-name" placeholder="ds18b20_eth_01">
    </div>
  </div>
  <button onclick="saveConfig()">&#128190; Salvar Configuração</button>
  <button class="btn-warn" onclick="doScan()" style="margin-left:8px">&#128270; Re-escanear Sensores</button>
  <div id="msg"></div>
</div>

<script>
const loadStatus = async () => {
  try{
    const r=await fetch('/api/status');
    const d=await r.json();

    document.getElementById('devname').textContent='Dispositivo: '+d.device;
    document.getElementById('uptime-val').textContent=formatUptime(d.uptime);
    document.getElementById('heap-val').textContent=d.free_heap+' bytes';

    // Rede
    const eth=d.eth||{};
    document.getElementById('net-info').innerHTML=
      `<b>IP:</b> ${eth.ip||'?'}<br><b>Gateway:</b> ${eth.gateway||'?'}<br>`+
      `<b>Link:</b> <span class="badge ${eth.link==='ON'?'badge-on':'badge-off'}">${eth.link||'?'}</span>`;

    // Buses
    renderBus('bus1-body', d.bus1||[]);
    renderBus('bus2-body', d.bus2||[]);

    // Diagnóstico
    loadDiag();
  }catch(e){console.error(e);}
}

const loadDiag = async () => {
  try{
    const r=await fetch('/api/diag');
    const d=await r.json();
    renderDiag('diag1', d.bus1);
    renderDiag('diag2', d.bus2);
  }catch(e){}
};

const renderDiag = (id, data) => {
  if(!data) return;
  const pct=data.quality_pct;
  const color=pct>=90?'#4caf50':pct>=60?'#ff9800':'#f44336';
  document.getElementById(id).innerHTML=
    `<span>${data.responding}/${data.discovered} sensores respondendo &mdash; `+
    `<strong style="color:${color}">${pct}%</strong> &mdash; ${data.note}</span>`+
    `<div class="quality-bar-bg"><div class="quality-bar" style="width:${pct}%;background:${color}"></div></div>`;
};

const renderBus = (tbodyId, sensors) => {
  const tb=document.getElementById(tbodyId);
  if(!sensors||sensors.length===0){
    tb.innerHTML='<tr><td colspan="3" style="color:#888">Nenhum sensor encontrado</td></tr>';
    return;
  }
  tb.innerHTML=sensors.map(s=>{
    const temp=s.error?'—':s.temp.toFixed(1)+' °C';
    const cls=s.error?'err':(s.temp>80?'warn':'ok');
    const status=s.error?`<span class="err">&#10060; ${s.error}</span>`:'<span class="ok">&#10004; OK</span>';
    return `<tr><td style="font-family:monospace;font-size:.9em">${s.addr}</td>`+
           `<td class="${cls}" style="font-size:1.1em">${temp}</td>`+
           `<td>${status}</td></tr>`;
  }).join('');
};

const formatUptime = (s) => {
  const h=Math.floor(s/3600), m=Math.floor((s%3600)/60), sec=s%60;
  return `${h}h ${m}m ${sec}s`;
};

const saveConfig = async () => {
  const body={
    serverHost: document.getElementById('cfg-host').value,
    serverPort: parseInt(document.getElementById('cfg-port').value)||3000,
    serverPath: document.getElementById('cfg-path').value,
    pollInterval: parseInt(document.getElementById('cfg-poll').value)||10000,
    deviceName: document.getElementById('cfg-name').value
  };
  try{
    const r=await fetch('/api/config',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(body)});
    const d=await r.json();
    showMsg(d.ok?'&#10004; Salvo com sucesso!':'&#10060; Erro ao salvar');
  }catch(e){showMsg('&#10060; Erro de conexão');}
};

const doScan = async () => {
  showMsg('&#128270; Escaneando...');
  try{
    await fetch('/api/scan',{method:'POST'});
    showMsg('&#10004; Scan concluído!');
    setTimeout(loadStatus,1000);
  }catch(e){showMsg('&#10060; Erro');}
};

const showMsg = (txt) => {
  const el=document.getElementById('msg');
  el.innerHTML=txt;
  setTimeout(()=>el.innerHTML='',4000);
};

const loadConfig = async () => {
  try{
    const r=await fetch('/api/status');
    const d=await r.json();
    if(d.config){
      document.getElementById('cfg-host').value=d.config.serverHost||'';
      document.getElementById('cfg-port').value=d.config.serverPort||'';
      document.getElementById('cfg-path').value=d.config.serverPath||'';
      document.getElementById('cfg-poll').value=d.config.pollInterval||'';
      document.getElementById('cfg-name').value=d.config.deviceName||'';
    }
  }catch(e){}
}

loadStatus();
loadConfig();
setInterval(loadStatus, 5000);
</script>
</body>
</html>)rawhtml");

  c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n"));
  c.printf("Content-Length: %u\r\n", html.length());
  c.print(F("Connection: close\r\n\r\n"));
  c.print(html);
}

// -------------------------------------------------------
// GET /api/status
// -------------------------------------------------------
static void handleApiStatus(EthernetClient& c) {
  // Leitura ao vivo (não usa cache) para /api/status
  String readings = buildReadingsJson();

  StaticJsonDocument<3584> doc;
  deserializeJson(doc, readings);

  // Injeta status de rede
  StaticJsonDocument<256> ethDoc;
  deserializeJson(ethDoc, ethernetStatusJson());
  doc["eth"] = ethDoc;
  doc["free_heap"] = ESP.getFreeHeap();

  // Injeta configuração atual (sem MAC)
  JsonObject cfgObj = doc.createNestedObject("config");
  cfgObj["serverHost"]   = cfg.serverHost;
  cfgObj["serverPort"]   = cfg.serverPort;
  cfgObj["serverPath"]   = cfg.serverPath;
  cfgObj["pollInterval"] = cfg.pollInterval;
  cfgObj["deviceName"]   = cfg.deviceName;

  String out;
  serializeJson(doc, out);
  sendOK(c, out);
}

// -------------------------------------------------------
// GET /api/diag
// -------------------------------------------------------
static void handleApiDiag(EthernetClient& c) {
  StaticJsonDocument<512> doc;

  StaticJsonDocument<256> d1, d2;
  deserializeJson(d1, busDiagJson(1));
  deserializeJson(d2, busDiagJson(2));
  doc["bus1"] = d1;
  doc["bus2"] = d2;

  String out;
  serializeJson(doc, out);
  sendOK(c, out);
}

// -------------------------------------------------------
// POST /api/config  – body JSON com campos opcionais
// -------------------------------------------------------
static void handleApiConfig(EthernetClient& c, const String& body) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { send400(c); return; }

  if (doc.containsKey("serverHost"))  strlcpy(cfg.serverHost, doc["serverHost"] | cfg.serverHost, sizeof(cfg.serverHost));
  if (doc.containsKey("serverPort"))  cfg.serverPort   = doc["serverPort"]   | cfg.serverPort;
  if (doc.containsKey("serverPath"))  strlcpy(cfg.serverPath, doc["serverPath"] | cfg.serverPath, sizeof(cfg.serverPath));
  if (doc.containsKey("pollInterval")) cfg.pollInterval = doc["pollInterval"] | cfg.pollInterval;
  if (doc.containsKey("deviceName")) strlcpy(cfg.deviceName, doc["deviceName"] | cfg.deviceName, sizeof(cfg.deviceName));

  cfg.magic = EEPROM_MAGIC;
  configSave(cfg);

  sendOK(c, F("{\"ok\":true}"));
}

// -------------------------------------------------------
// POST /api/scan
// -------------------------------------------------------
static void handleApiScan(EthernetClient& c) {
  scanBuses();
  httpPostAddresses();
  sendOK(c, F("{\"ok\":true,\"bus1\":0,\"bus2\":0}"));
}

// -------------------------------------------------------
// Lê body de requisição POST (até 1 KB)
// -------------------------------------------------------
static String readBody(EthernetClient& c, int contentLength) {
  String body = "";
  if (contentLength <= 0 || contentLength > 1024) return body;
  unsigned long t = millis();
  while ((int)body.length() < contentLength && millis() - t < 3000) {
    if (c.available()) body += (char)c.read();
  }
  return body;
}

// -------------------------------------------------------
// Despacha requisição recebida
// -------------------------------------------------------
static void dispatchRequest(EthernetClient& c, const String& method,
                             const String& path, const String& body) {
  Serial.printf("[WEB] %s %s\n", method.c_str(), path.c_str());

  if (path == "/" || path == "/index.html") {
    handleRoot(c);
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
// Chamado no loop() principal
// -------------------------------------------------------
void handleWebClient() {
  EthernetClient client = webServer.available();
  if (!client) return;

  unsigned long t = millis();
  String reqLine = "";
  String headers = "";
  int    contentLength = 0;

  // Lê linha de status e headers
  while (client.connected() && millis() - t < 3000) {
    if (!client.available()) continue;
    String line = client.readStringUntil('\n');
    line.trim();
    if (reqLine.isEmpty()) {
      reqLine = line;
    } else if (line.isEmpty()) {
      break;   // fim dos headers
    } else {
      String lower = line;
      lower.toLowerCase();
      if (lower.startsWith("content-length:")) {
        String val = lower.substring(15); val.trim(); contentLength = val.toInt();
      }
    }
  }

  if (reqLine.isEmpty()) { client.stop(); return; }

  // Parse "METHOD /path HTTP/1.x"
  int s1 = reqLine.indexOf(' ');
  int s2 = reqLine.indexOf(' ', s1 + 1);
  if (s1 < 0 || s2 < 0) { client.stop(); return; }

  String method = reqLine.substring(0, s1);
  String path   = reqLine.substring(s1 + 1, s2);

  // Lê body se POST
  String body = (method == "POST" && contentLength > 0)
                ? readBody(client, contentLength)
                : "";

  dispatchRequest(client, method, path, body);

  delay(5);
  client.stop();
}
