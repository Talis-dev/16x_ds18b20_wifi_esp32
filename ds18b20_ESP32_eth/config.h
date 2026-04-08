#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// -------------------------------------------------------
// Estrutura de configuração persistida na EEPROM interna
// -------------------------------------------------------
#define EEPROM_SIZE   256
#define EEPROM_MAGIC  0xA5   // byte de validação

struct Config {
  uint8_t  magic;                    // deve ser EEPROM_MAGIC para validade
  char     serverHost[64];           // ex: "192.168.1.100"
  uint16_t serverPort;               // ex: 3000
  char     serverPath[64];           // ex: "/api/sensors"
  uint32_t pollInterval;             // ms entre leituras (padrão 10000)
  char     deviceName[32];           // nome/id do dispositivo
  uint8_t  mac[6];                   // MAC address customizável
};

// Valores padrão
static const Config CONFIG_DEFAULT = {
  EEPROM_MAGIC,
  "192.168.1.100",
  3000,
  "/api/sensors",
  10000,
  "ds18b20_eth_01",
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 }
};

// -------------------------------------------------------
// Carrega configuração da EEPROM.
// Se inválida, grava os valores padrão.
// -------------------------------------------------------
inline void configLoad(Config& cfg) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  if (cfg.magic != EEPROM_MAGIC) {
    Serial.println(F("[CFG] EEPROM inválida – gravando padrão"));
    cfg = CONFIG_DEFAULT;
    EEPROM.put(0, cfg);
    EEPROM.commit();
  }
  Serial.printf("[CFG] Host: %s:%u  Path: %s  Poll: %lums\n",
                cfg.serverHost, cfg.serverPort, cfg.serverPath, cfg.pollInterval);
}

// -------------------------------------------------------
// Persiste configuração na EEPROM
// -------------------------------------------------------
inline void configSave(const Config& cfg) {
  EEPROM.put(0, cfg);
  EEPROM.commit();
  Serial.println(F("[CFG] Configuração salva na EEPROM"));
}
