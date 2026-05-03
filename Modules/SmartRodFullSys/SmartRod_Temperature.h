#ifndef SMARTROD_TEMPERATURE_H
#define SMARTROD_TEMPERATURE_H

#include "SmartRod_Globals.h"

// -------------------- Temperature --------------------

static void updateWaterTemperature(unsigned long now) {
  if (now - lastTempMs >= TEMP_SAMPLE_MS) {
    lastTempMs = now;

    waterTempSensor.requestTemperatures();
    float tC = waterTempSensor.getTempCByIndex(0);

    if (tC != DEVICE_DISCONNECTED_C) {
      waterTempC = tC;
    } else {
      waterTempC = DEVICE_DISCONNECTED_C;
    }
  }
}

#endif
