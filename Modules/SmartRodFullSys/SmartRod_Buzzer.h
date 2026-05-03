#ifndef SMARTROD_BUZZER_H
#define SMARTROD_BUZZER_H

#include "SmartRod_Globals.h"

// Active buzzer drive helpers.
static void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

static void buzzerOn() {
  digitalWrite(BUZZER_PIN, HIGH);
}

static void buzzerPulse(unsigned long onMs, unsigned long offMs) {
  buzzerOn();
  delay(onMs);
  buzzerOff();
  delay(offMs);
}

// Short blocking alert sequence used only on bite events.
static void playBiteAlert() {
  buzzerPulse(120, 80);
  buzzerPulse(120, 80);
  buzzerPulse(180, 0);
}

#endif
