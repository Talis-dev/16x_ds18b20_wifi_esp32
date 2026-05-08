#pragma once
#include <Arduino.h>
#include <Preferences.h>

// -------------------------------------------------------
// Estrutura de configuracao MQTT persistida no NVS (Preferences)
// -------------------------------------------------------
struct Config {
  char     mqttServer[64];    // ex: "192.168.1.100"
  uint16_t mqttPort;          // ex: 1883
  char     mqttClientId[32];  // ex: "sensors_ds18b20"
  char     mqttUser[32];      // usuario MQTT (vazio = sem autenticacao)
  char     mqttPass[64];      // senha MQTT
  char     deviceName[32];    // nome do dispositivo
  uint32_t pollInterval;      // ms entre leituras (padrao 10000)
};

// -------------------------------------------------------
// Carrega configuracao do NVS.
// Se nao houver dados, usa valores padrao.
// -------------------------------------------------------
inline void configLoad(Config& cfg) {
  Preferences prefs;
  prefs.begin("mqtt_cfg", true); // read-only
  strlcpy(cfg.mqttServer,   prefs.getString("server",   "192.168.1.100").c_str(), sizeof(cfg.mqttServer));
  cfg.mqttPort =             prefs.getUShort("port",    1883);
  strlcpy(cfg.mqttClientId, prefs.getString("clientid", "sensors_ds18b20").c_str(), sizeof(cfg.mqttClientId));
  strlcpy(cfg.mqttUser,     prefs.getString("user",    "").c_str(), sizeof(cfg.mqttUser));
  strlcpy(cfg.mqttPass,     prefs.getString("pass",    "").c_str(), sizeof(cfg.mqttPass));
  strlcpy(cfg.deviceName,   prefs.getString("name",    "ds18b20_wifi_01").c_str(), sizeof(cfg.deviceName));
  cfg.pollInterval =         prefs.getULong("poll",    10000);
  prefs.end();
  Serial.printf("[CFG] MQTT: %s:%u  ClientID: %s  Poll: %lums\n",
                cfg.mqttServer, cfg.mqttPort, cfg.mqttClientId, cfg.pollInterval);
}

// -------------------------------------------------------
// Persiste configuracao no NVS
// -------------------------------------------------------
inline void configSave(const Config& cfg) {
  Preferences prefs;
  prefs.begin("mqtt_cfg", false); // read-write
  prefs.putString("server",   cfg.mqttServer);
  prefs.putUShort("port",     cfg.mqttPort);
  prefs.putString("clientid", cfg.mqttClientId);
  prefs.putString("user",     cfg.mqttUser);
  prefs.putString("pass",     cfg.mqttPass);
  prefs.putString("name",     cfg.deviceName);
  prefs.putULong("poll",      cfg.pollInterval);
  prefs.end();
  Serial.println(F("[CFG] Configuracao salva no NVS"));
}
