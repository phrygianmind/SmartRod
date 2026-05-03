#ifndef SMARTROD_STATE_MACHINE_H
#define SMARTROD_STATE_MACHINE_H

#include "SmartRod_Globals.h"

// ==================== State Entry Helpers ====================

static void enterArmed(unsigned long now) {
  state = ARMED;
  castCandidateStartMs = 0;
  castQuietStartMs = 0;

  biteBannerUntil = 0;
  biteLockoutUntil = 0;

  biteEnableAtMs = now + 300;

  //noInterrupts();
  //pulseCount = 0;
  //interrupts();

  //savedCastDistance = 0.0f;
}

static void enterCasting(unsigned long now) {
  state = CASTING;
  castCandidateStartMs = 0;
  castQuietStartMs = 0;

  forceNewtons = 0.0f;
  forceHold = 0.0f;
  forceUpdated = false;
  holdUntilMs = 0;

  biteEnableAtMs = now + BITE_DISABLED_DURING_CAST_MS;

  noInterrupts();
  pulseCount = 0;
  interrupts();

  savedCastDistance = 0.0f;
}

static void enterWaitBite(unsigned long now) {
  state = WAIT_BITE;
  castCandidateStartMs = 0;
  castQuietStartMs = 0;

  biteEnableAtMs = now + POST_CAST_SETTLE_MS;
}

static void enterReeling(unsigned long now) {
  state = REELING;
  castCandidateStartMs = 0;
  castQuietStartMs = 0;

  noInterrupts();
  pulseCount = 0;
  interrupts();

  savedCastDistance = 0.0f;

  // Power-bar logic is only meaningful for casting.
  forceNewtons = 0.0f;
  forceHold = 0.0f;
  forceUpdated = false;
  holdUntilMs = 0;

  biteLockoutUntil = now + BITE_LOCKOUT_MS;
}

// ==================== State Machine Update ====================

static void updateStateMachine(unsigned long now) {
  if (state == ARMED) {
    if (imuDynAccel >= CAST_START_DYN) {
      if (castCandidateStartMs == 0) castCandidateStartMs = now;

      if (now - castCandidateStartMs >= CAST_CONFIRM_MS) {
        enterCasting(now);
      }
    } else {
      castCandidateStartMs = 0;
    }
    return;
  }

  if (state == CASTING) {
    if (imuDynAccel <= CAST_END_DYN) {
      if (castQuietStartMs == 0) castQuietStartMs = now;

      if (now - castQuietStartMs >= CAST_END_QUIET_MS) {
        enterWaitBite(now);
      }
    } else {
      castQuietStartMs = 0;
    }
    return;
  }

  if (state == WAIT_BITE) {
    return;
  }

  if (state == REELING) {
    return;
  }
}

#endif
