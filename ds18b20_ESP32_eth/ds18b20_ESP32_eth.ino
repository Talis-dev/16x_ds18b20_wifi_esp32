/*
 * DS18B20 Multi-Bus - ESP32 + W5500 Ethernet
 * ============================================
 * Lê sensores DS18B20 em 2 barramentos OneWire e envia via HTTP POST.
 * Servidor web embutido para configuração e monitoramento.
 *
 * Pinagem W5500:
 *   MOSI → GPIO 23 | MISO → GPIO 19 | SCLK → GPIO 18
 *   CS   → GPIO 5  | RST  → GPIO 33 | INT  → GPIO 4
 *
 * Barramentos OneWire:
 *   Bus 1 → GPIO 25
 *   Bus 2 → GPIO 26
 *
 * LEDs:
 *   LED Status Rede  → GPIO 12
 *   LED Atividade B1 → GPIO 14
 *   LED Atividade B2 → GPIO 27
 */

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "config.h"

// -------------------------------------------------------
// Pinos OneWire
// -------------------------------------------------------
#define ONE_WIRE_BUS_1  25
#define ONE_WIRE_BUS_2  26

// -------------------------------------------------------
// Pinos LEDs
// -------------------------------------------------------
#define LED_NET   12   // Status Rede / W5500
#define LED_BUS1  27   // Atividade Barramento 1
#define LED_BUS2  14   // Atividade Barramento 2

// -------------------------------------------------------
// Pinos W5500
// -------------------------------------------------------
#define ETH_CS   5
#define ETH_RST  33
#define ETH_INT  4

// -------------------------------------------------------
// OneWire / DallasTemperature
// -------------------------------------------------------
OneWire          ow1(ONE_WIRE_BUS_1);
OneWire          ow2(ONE_WIRE_BUS_2);
DallasTemperature bus1(&ow1);
DallasTemperature bus2(&ow2);

int devCount1 = 0;
int devCount2 = 0;

// Endereços descobertos (até 16 por barramento)
DeviceAddress addrBus1[16];
DeviceAddress addrBus2[16];

// -------------------------------------------------------
// Estado global
// -------------------------------------------------------
Config cfg;
unsigned long lastPoll  = 0;
bool          firstScan = true;     // envia lista na inicialização

EthernetServer webServer(80);

// Forward declarations (necessário pela ordem alfabética dos .ino)
void handleWebClient();
void ethernetSetup();
void ethernetMaintain();
void scanBuses();
void httpPostAddresses();
void httpPostReadings();
void updateReadingsCache();
void ledSetup();
void ledSet(int pin, int state);
void ledBlink(int pin, int times, int ms);
extern String g_readingsCache;

// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n== DS18B20 ETH v2 =="));

  // LEDs – active-low: HIGH = apagado, LOW = aceso
  ledSetup();   // ledSetup já coloca todos em HIGH (apagado)

  // Carrega config da EEPROM
  configLoad(cfg);

  // Inicializa W5500
  ethernetSetup();

  // Inicializa servidores
  webServer.begin();

  // OneWire scan inicial
  scanBuses();

  // Popula cache imediatamente para a página web ter dados
  updateReadingsCache();

  // Envia lista de endereços encontrados
  httpPostAddresses();

  Serial.println(F("Setup completo. Iniciando loop..."));
}

// -------------------------------------------------------
void loop() {
  // Atende requisições web
  handleWebClient();

  // Polling dos sensores
  if (millis() - lastPoll >= (unsigned long)cfg.pollInterval) {
    lastPoll = millis();
    httpPostReadings();
  }
}
