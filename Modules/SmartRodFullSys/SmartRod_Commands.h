#ifndef SMARTROD_COMMANDS_H
#define SMARTROD_COMMANDS_H

#include "SmartRod_Globals.h"
#include "SmartRod_Buzzer.h"
#include "SmartRod_Display.h"
#include "SmartRod_Labels.h"
#include "SmartRod_StateMachine.h"

// -------------------- Reset --------------------

static void resetModel() {
  noInterrupts();
  pulseCount = 0;
  lastTriggerTime = 0;
  hallPulseFlag = false;
  interrupts();

  forceNewtons = 0.0f;
  forceHold = 0.0f;
  imuDynAccel = 0.0f;
  amagLP = 0.0f;
  forceUpdated = false;
  holdUntilMs = 0;

  piezoBaseline = 0;
  piezoSpike = 0;
  biteLockoutUntil = 0;
  biteBannerUntil = 0;

  savedCastDistance = 0.0f;

  buzzerOff();
  enterArmed(millis());

  showResetScreen();
}

// -------------------- Command Handling --------------------

static void finalizeReelingAndRearm() {
  Serial.print("Final distance (m): ");
  Serial.println(savedCastDistance, 2);

  if (SerialBT.hasClient()) {
    SerialBT.print("FINAL,");
    SerialBT.println(savedCastDistance, 1);
  }

  enterArmed(millis());
}

static void handleCommand(char ch) {
  if (ch == 'r' || ch == 'R') {
    resetModel();
    Serial.println("Reset.");
  } else if (ch == 'a' || ch == 'A') {
    enterArmed(millis());
    Serial.println("Re-armed.");
  } else if (ch == '1') {
    sensIdx = 0;
    Serial.println("Piezo sensitivity: HIGH");
  } else if (ch == '2') {
    sensIdx = 1;
    Serial.println("Piezo sensitivity: MED");
  } else if (ch == '3') {
    sensIdx = 2;
    Serial.println("Piezo sensitivity: LOW");
  } else if (ch == 'x' || ch == 'X' || ch == 'd' || ch == 'D') {
    if (state == REELING) {
      finalizeReelingAndRearm();
      Serial.println("Reeling done. Final distance stored/sent.");
    } else {
      Serial.println("DONE ignored: not currently reeling.");
    }
  } else if (ch == '?') {
    Serial.print("dyn=");
    Serial.print(imuDynAccel, 3);
    Serial.print(" force=");
    Serial.print(forceNewtons, 3);
    Serial.print(" hold=");
    Serial.print(forceHold, 3);
    Serial.print(" state=");
    Serial.print(stateLabel(state, millis() < biteBannerUntil));
    Serial.print(" tempC=");
    if (waterTempC == DEVICE_DISCONNECTED_C) {
      Serial.println("NA");
    } else {
      Serial.println(waterTempC, 2);
    }
  }
}

#endif
