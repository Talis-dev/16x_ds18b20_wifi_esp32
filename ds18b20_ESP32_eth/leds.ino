/*
 * leds.ino
 * Controle dos 3 LEDs de feedback visual.
 */

// -------------------------------------------------------
void ledSetup() {
  pinMode(LED_NET,  OUTPUT);
  pinMode(LED_BUS1, OUTPUT);
  pinMode(LED_BUS2, OUTPUT);
  digitalWrite(LED_NET,  LOW);
  digitalWrite(LED_BUS1, LOW);
  digitalWrite(LED_BUS2, LOW);
}

void ledSet(int pin, int state) {
  digitalWrite(pin, state);
}

// Pisca 'times' vezes com intervalo 'ms' (bloqueante)
void ledBlink(int pin, int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(ms);
    digitalWrite(pin, LOW);
    delay(ms);
  }
}
