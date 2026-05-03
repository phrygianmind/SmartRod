#ifndef SMARTROD_DISPLAY_H
#define SMARTROD_DISPLAY_H

#include "SmartRod_Globals.h"
#include "SmartRod_Labels.h"

// -------------------- Nokia UI --------------------

static void drawNokiaUI(float distance_m, float forceHeldN,
                        RodState st, bool biteBanner, uint8_t sensitivity,
                        float tempC) {
  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("State: ");
  display.print(stateLabel(st, biteBanner));

  display.setCursor(0, 10);
  display.print("Line:");
  display.print(distance_m, 1);
  display.print("m");

  display.setCursor(0, 20);
  display.print("Temp:");
  if (tempC == DEVICE_DISCONNECTED_C) {
    display.print("--");
  } else {
    display.print(tempC, 1);
    display.print("C");
  }

  display.setCursor(0, 30);
  display.print("Sens:");
  display.print(sensLabel(sensitivity));

  float clamped = forceHeldN;
  if (clamped < 0.0f) clamped = 0.0f;
  if (clamped > MAX_FORCE_EXPECTED) clamped = MAX_FORCE_EXPECTED;

  const int barX = 0;
  const int barY = 40;
  const int barW = 84;
  const int barH = 8;

  display.drawRect(barX, barY, barW, barH, BLACK);

  int fillW = (int)lroundf((clamped / MAX_FORCE_EXPECTED) * (float)(barW - 2));
  if (fillW < 0) fillW = 0;
  if (fillW > (barW - 2)) fillW = (barW - 2);

  if (fillW > 0) {
    display.fillRect(barX + 1, barY + 1, fillW, barH - 2, BLACK);
  }

  display.display();
}

static void showResetScreen() {
  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Reset");
  display.display();
}

static void showMpuFailScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("MPU6050 FAIL");
  display.display();
}

#endif
