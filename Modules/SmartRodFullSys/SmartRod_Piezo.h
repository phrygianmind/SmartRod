#ifndef SMARTROD_PIEZO_H
#define SMARTROD_PIEZO_H

#include "SmartRod_Globals.h"
#include "SmartRod_Buzzer.h"
#include "SmartRod_StateMachine.h"

static void calibratePiezoBaseline() {
  int sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(PIEZO_PIN);
    delay(2);
  }
  piezoBaseline = sum / 50;
}

static void updatePiezoBite(unsigned long now) {
  int v = analogRead(PIEZO_PIN);

  if (piezoBaseline == 0) piezoBaseline = v;
  piezoBaseline = (int)((1.0f - PIEZO_ALPHA) * piezoBaseline + PIEZO_ALPHA * v);
  piezoSpike = abs(v - piezoBaseline);

  if (state == WAIT_BITE && now >= biteEnableAtMs && now >= biteLockoutUntil) {
    if (piezoSpike > BITE_THRESH[sensIdx]) {
      unsigned long bannerUntil = now + BITE_BANNER_MS;
      unsigned long lockoutUntil = now + BITE_LOCKOUT_MS;

      Serial.println("BITE!");
      playBiteAlert();

      biteBannerUntil = bannerUntil;
      biteLockoutUntil = lockoutUntil;

      if (AUTO_REARM_AFTER_BITE) {
        enterArmed(millis());
        biteBannerUntil = bannerUntil;
        biteLockoutUntil = lockoutUntil;
      } else {
        enterReeling(millis());
        biteBannerUntil = bannerUntil;
        biteLockoutUntil = lockoutUntil;
        Serial.println("Auto-transition: WAIT_BITE -> REELING");
      }
    }
  }
}

#endif
