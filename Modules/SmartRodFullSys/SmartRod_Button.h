#ifndef SMARTROD_BUTTON_H
#define SMARTROD_BUTTON_H

#include "SmartRod_Globals.h"
#include "SmartRod_Labels.h"

static void cycleSensitivity() {
  sensIdx++;
  if (sensIdx > 2) sensIdx = 0;

  Serial.print("Piezo sensitivity: ");
  Serial.println(sensLabel(sensIdx));
}

static void updateButton(unsigned long now) {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonEdgeMs = now;
    lastButtonReading = reading;
  }

  if ((now - lastButtonEdgeMs) >= BUTTON_DEBOUNCE_MS) {
    if (reading != buttonStableState) {
      buttonStableState = reading;

      if (buttonStableState == LOW) {
        cycleSensitivity();
      }
    }
  }
}

#endif
