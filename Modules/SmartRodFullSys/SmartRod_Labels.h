#ifndef SMARTROD_LABELS_H
#define SMARTROD_LABELS_H

#include "SmartRod_Globals.h"

// -------------------- Helpers --------------------

static const char* stateLabel(RodState st, bool biteBanner) {
  if (biteBanner) return "BITE";

  switch (st) {
    case ARMED:     return "ARM";
    case CASTING:   return "CAST";
    case WAIT_BITE: return "WAIT";
    case REELING:   return "REEL";
    default:        return "?";
  }
}

static const char* sensLabel(uint8_t idx) {
  switch (idx) {
    case 0: return "HIGH";
    case 1: return "MED";
    case 2: return "LOW";
    default: return "?";
  }
}

#endif
