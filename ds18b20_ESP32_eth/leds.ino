/*
 * leds.ino
 * Controle dos 3 LEDs de feedback visual.
 * LEDS NORMALMENTE desligado (HIGH) e são ligados (LOW) para indicar atividade ou status:
 * LED_NET: indica status da conexão Ethernet (on = link ativo)
 * LED_BUS1: indica atividade de leitura do barramento 1 (on = lendo)
 * LED_BUS2: indica atividade de leitura do barramento 2 (on = lendo)
 */

// -------------------------------------------------------
void ledSetup() {
  pinMode(LED_NET,  OUTPUT);
  pinMode(LED_BUS1, OUTPUT);
  pinMode(LED_BUS2, OUTPUT);
  digitalWrite(LED_NET,  HIGH);
  digitalWrite(LED_BUS1, HIGH);
  digitalWrite(LED_BUS2, HIGH);
}

void ledSet(int pin, int state) {
  digitalWrite(pin, state);
}

// Pisca 'times' vezes com intervalo 'ms' (bloqueante)
void ledBlink(int pin, int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, LOW);
    delay(ms);
    digitalWrite(pin, HIGH);
    delay(ms);
  }
}
