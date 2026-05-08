
int  reboot     = 0;
bool mqttFailed = false;  // true apos 10 falhas - para de tentar

void reconnect() {
  // Nao bloqueante: tenta uma vez por chamada.
  // O loop() continua rodando (web server, sensores, etc.)

  if (mqttFailed) return;  // parou de tentar - usuario deve corrigir via web

  if (WiFi.status() != WL_CONNECTED) return;  // WiFi fora - handleWiFiDisconnect cuida

  Serial.println(F("MQTT connecting..."));

  bool mqttOk = (strlen(cfg.mqttUser) > 0)
    ? client.connect(cfg.mqttClientId, cfg.mqttUser, cfg.mqttPass)
    : client.connect(cfg.mqttClientId);

  if (mqttOk) {
    Serial.println(F("MQTT connected!"));
    reboot = 0;
    client.publish("sensors_ds18b20_esp32", "sensors_ds18b20_esp32 Online");
    client.subscribe("reset_ds18b20_esp32");
    client.subscribe("timeToSend_ds18b20_esp32");
  } else {
    Serial.print(F("MQTT FALHA, status="));
    Serial.print(client.state());
    Serial.print(F(" tentativa "));
    Serial.println(reboot + 1);
    reboot++;
    if (reboot >= 10) {
      mqttFailed = true;
      Serial.println(F("MQTT: 10 falhas. Parou de tentar. Configure o servidor via pagina web."));
    }
  }
}
