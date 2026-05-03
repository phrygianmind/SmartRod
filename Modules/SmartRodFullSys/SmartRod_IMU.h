#ifndef SMARTROD_IMU_H
#define SMARTROD_IMU_H

#include "SmartRod_Globals.h"

static void updateImu() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float amag = sqrtf(ax * ax + ay * ay + az * az);

  if (amagLP == 0.0f) amagLP = amag;
  amagLP = LP_ALPHA * amagLP + (1.0f - LP_ALPHA) * amag;

  imuDynAccel = fabsf(amag - amagLP);

  forceNewtons = MASS_KG * imuDynAccel;
  forceUpdated = true;
}

static void updatePeakHold(unsigned long now) {
  if (forceUpdated) {
    if (state == CASTING) {
      if (forceNewtons > forceHold) {
        forceHold = forceNewtons;
        holdUntilMs = now + HOLD_MS;
      }
    }
    forceUpdated = false;
  }

  if (now >= holdUntilMs) {
    forceHold *= HOLD_DECAY_PER_UPDATE;
    if (forceHold < HOLD_FLOOR) forceHold = 0.0f;
  }
}

#endif
