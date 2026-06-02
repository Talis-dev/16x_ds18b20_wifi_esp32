
int   reboot            = 0;
bool  mqttFailed        = false;   // true = houve falha (exibido na UI; NAO para tentativas)
bool  mqttEverConnected = false;   // dispara envio de device list apos 1a conexao

static unsigned long s_mqttDisconnectedSince = 0;
unsigned long g_mqttRetryInterval = 5000;   // backoff: 5s -> 60s (lido pelo loop principal)

#define MQTT_WATCHDOG_MS  (30UL * 60UL * 1000UL)  // 30 min sem MQTT -> reinicia ESP
#define MQTT_RETRY_MAX_MS  60000UL                  // teto do backoff

void reconnect() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println(F("MQTT connecting..."));

  bool ok = (strlen(cfg.mqttUser) > 0)
    ? client.connect(cfg.mqttClientId, cfg.mqttUser, cfg.mqttPass)
    : client.connect(cfg.mqttClientId);

  if (ok) {
    Serial.println(F("MQTT connected!"));
    reboot                  = 0;
    mqttFailed              = false;
    mqttEverConnected       = true;
    s_mqttDisconnectedSince = 0;
    g_mqttRetryInterval     = 5000;  // reset backoff
    client.publish("sensors_ds18b20_esp32", "sensors_ds18b20_esp32 Online");
    client.subscribe("reset_ds18b20_esp32");
    client.subscribe("timeToSend_ds18b20_esp32");
  } else {
    reboot++;
    mqttFailed = true;

    // Backoff exponencial: 5s, 10s, 20s, 40s, 60s (max)
    g_mqttRetryInterval = min(g_mqttRetryInterval * 2, MQTT_RETRY_MAX_MS);

    Serial.print(F("MQTT FALHA, status="));
    Serial.print(client.state());
    Serial.print(F("  tentativa="));
    Serial.print(reboot);
    Serial.print(F("  proxima em "));
    Serial.print(g_mqttRetryInterval / 1000);
    Serial.println(F("s"));

    // Watchdog: 30 min sem MQTT -> reinicia o ESP para recuperar conexao
    if (s_mqttDisconnectedSince == 0) s_mqttDisconnectedSince = millis();
    if (millis() - s_mqttDisconnectedSince >= MQTT_WATCHDOG_MS) {
      Serial.println(F("MQTT watchdog: 30min sem conexao. Reiniciando ESP..."));
      delay(500);
      ESP.restart();
    }
  }
}

// Chamado pela pagina web: forca nova tentativa imediata
void mqttForceReconnect() {
  mqttFailed              = false;
  reboot                  = 0;
  s_mqttDisconnectedSince = 0;
  g_mqttRetryInterval     = 5000;
  client.disconnect();
  delay(100);
  Serial.println(F("[MQTT] Reconexao forcada pelo usuario"));
}
