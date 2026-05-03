#ifndef SMARTROD_HALL_H
#define SMARTROD_HALL_H

#include "SmartRod_Globals.h"

// Hall pulse ISR with debounce.
// Pulse acceptance is deferred to loop() so counting can be state-gated safely.
void IRAM_ATTR hall_ISR() {
  unsigned long currentTime = millis();

  if (currentTime - lastTriggerTime > 200) {
    lastTriggerTime = currentTime;
    hallPulseFlag = true;
  }
}

static void consumeHallPulse() {
  if (hallPulseFlag) {
    noInterrupts();
    hallPulseFlag = false;
    interrupts();

    if (state == REELING) {
      pulseCount++;
    }
  }
}

#endif
