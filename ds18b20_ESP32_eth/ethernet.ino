/*
 * ethernet.ino
 * Inicialização e monitoramento do módulo W5500 via SPI.
 */

// Referências declaradas no arquivo principal
extern Config cfg;
extern EthernetServer webServer;

// Tempo da última checagem de link
static unsigned long lastEthCheck = 0;
#define ETH_CHECK_INTERVAL 5000

// -------------------------------------------------------
void ethernetSetup() {
  // Reset hardware do W5500
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  Ethernet.init(ETH_CS);

  Serial.print(F("[ETH] Iniciando DHCP..."));
  ledBlink(LED_NET, 3, 150);

  if (Ethernet.begin(cfg.mac) == 0) {
    Serial.println(F(" FALHA DHCP. Usando IP fixo de fallback."));
    // IP de fallback: 192.168.3.220
    IPAddress fallback(192, 168, 3, 220);
    IPAddress gateway(192, 168, 3, 1);
    IPAddress subnet(255, 255, 255, 0);
    Ethernet.begin(cfg.mac, fallback, gateway, gateway, subnet);
  }

  // Aguarda link físico
  int linkTry = 0;
  while (Ethernet.linkStatus() == LinkOFF && linkTry < 20) {
    delay(500);
    Serial.print('.');
    linkTry++;
  }

  if (Ethernet.linkStatus() == LinkON) {
    ledSet(LED_NET, HIGH);
    Serial.println();
    Serial.print(F("[ETH] IP: "));
    Serial.println(Ethernet.localIP());
  } else {
    ledSet(LED_NET, LOW);
    Serial.println(F("\n[ETH] Sem link físico!"));
  }
}

// -------------------------------------------------------
// Mantém DHCP renovado e monitora link.
// Chame no loop() periodicamente.
// -------------------------------------------------------
void ethernetMaintain() {
  if (millis() - lastEthCheck < ETH_CHECK_INTERVAL) return;
  lastEthCheck = millis();

  // Renova DHCP se necessário
  switch (Ethernet.maintain()) {
    case 1: Serial.println(F("[ETH] Renovação DHCP falhou")); break;
    case 2: Serial.println(F("[ETH] DHCP renovado")); break;
    case 3: Serial.println(F("[ETH] Rebind DHCP falhou")); break;
    case 4: Serial.println(F("[ETH] DHCP rebind OK")); break;
    default: break;
  }

  if (Ethernet.linkStatus() == LinkON) {
    ledSet(LED_NET, HIGH);
  } else {
    ledSet(LED_NET, LOW);
    Serial.println(F("[ETH] Link perdido!"));
  }
}

// -------------------------------------------------------
// Retorna info de conexão como JSON string
// -------------------------------------------------------
String ethernetStatusJson() {
  StaticJsonDocument<200> doc;
  IPAddress ip  = Ethernet.localIP();
  IPAddress gw  = Ethernet.gatewayIP();
  IPAddress dns = Ethernet.dnsServerIP();

  char buf[20];
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  doc["ip"] = buf;
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
  doc["gateway"] = buf;
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", dns[0], dns[1], dns[2], dns[3]);
  doc["dns"] = buf;
  doc["link"] = (Ethernet.linkStatus() == LinkON) ? "ON" : "OFF";

  String out;
  serializeJson(doc, out);
  return out;
}
