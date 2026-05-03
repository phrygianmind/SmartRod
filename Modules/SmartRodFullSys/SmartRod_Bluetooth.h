#ifndef SMARTROD_BLUETOOTH_H
#define SMARTROD_BLUETOOTH_H

#include "SmartRod_Globals.h"
#include "SmartRod_Labels.h"

// ==================== APP DATA TRANSMISSION ====================

static void updateBluetoothTelemetry(unsigned long now) {
  if (now - lastBtTxMs < BT_TX_INTERVAL) {
    return;
  }

  lastBtTxMs = now;

  if (SerialBT.hasClient()) {
    char tempText[10];
    if (waterTempC == DEVICE_DISCONNECTED_C) {
      strncpy(tempText, "NA", sizeof(tempText));
      tempText[sizeof(tempText) - 1] = '\0';
    } else {
      snprintf(tempText, sizeof(tempText), "%.1f", waterTempC);
    }

    const bool biteBanner = (now < biteBannerUntil);
    const char* biteStatus = biteBanner ? "FISH" : "No";

    char packet[80];
    snprintf(packet, sizeof(packet), "%.2f,%s,%s,%s,%.1f,%s",
             forceHold,
             biteStatus,
             stateLabel(state, biteBanner),
             sensLabel(sensIdx),
             savedCastDistance,
             tempText);

    SerialBT.println(packet);

    Serial.print("[BT SENDING]: ");
    Serial.println(packet);
  }
  else {
    static unsigned long lastWait = 0;
    if (now - lastWait > 5000) {
      Serial.println("[BT STATUS]: Waiting for CyberFish app...");
      lastWait = now;
    }
  }
}

#endif
