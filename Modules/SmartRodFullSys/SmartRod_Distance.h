#ifndef SMARTROD_DISTANCE_H
#define SMARTROD_DISTANCE_H

#include "SmartRod_Globals.h"

// -------------------- Distance --------------------

static float updateDistance() {
  float distance_m = savedCastDistance;

  if (state == REELING) {
    int localPulseCount;
    noInterrupts();
    localPulseCount = pulseCount;
    interrupts();

    float revolutions = localPulseCount * PULSES_TO_REVOLUTIONS;
    distance_m = revolutions * CIRCUMFERENCE * CALIBRATION_FACTOR;
    savedCastDistance = distance_m;
  }

  return distance_m;
}

#endif
