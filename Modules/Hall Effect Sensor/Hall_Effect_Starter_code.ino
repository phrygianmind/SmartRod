#include <Arduino.h>

// ───────────────────────────────────────────────
// Hardware pin definition
#define HALL_PIN 27

// ───────────────────────────────────────────────
// Spool geometry and calibration (all in meters)
const float R_CORE      = 0.02413f;   // Core radius ≈ 0.95 inch
const float R_FULL      = 0.03048f;   // Full spool radius ≈ 1.2 inch
const float AVG_RADIUS  = (R_CORE + R_FULL) / 2.0f;
const float CIRCUMFERENCE = TWO_PI * AVG_RADIUS;   // meters per full revolution

const float MAGNETS_PER_REVOLUTION = 2.0f;         // Two magnets at 180° → 2 pulses per turn
const float PULSES_TO_REVOLUTIONS = 1.0f / MAGNETS_PER_REVOLUTION;

const float CALIBRATION_FACTOR = 1.00f;            // Fine-tune based on real-world measurement

const float METERS_TO_FEET = 3.28084f;

// ───────────────────────────────────────────────
// Volatile variables modified in ISR
volatile int      pulseCount     = 0;
volatile unsigned long lastTriggerTime = 0;

// ISR – triggered on FALLING edge (magnet detection)
void IRAM_ATTR hall_ISR() {
  unsigned long currentTime = millis();

  // 200 ms software debounce (suitable for manual testing; reduce if spool spins very fast)
  if (currentTime - lastTriggerTime > 200) {
    pulseCount++;
    lastTriggerTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);                // Allow serial to stabilize

  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hall_ISR, FALLING);

  Serial.println("\nSmart Rod – Line Length Measurement (Dual-Magnet Configuration)");
  Serial.printf("Avg radius:      %.4f m\n", AVG_RADIUS);
  Serial.printf("Circumference:   %.4f m per full revolution\n", CIRCUMFERENCE);
  Serial.printf("Magnets/rev:     %.0f → pulses-to-revs factor: %.3f\n", 
                MAGNETS_PER_REVOLUTION, PULSES_TO_REVOLUTIONS);
  Serial.printf("Calibration:     %.3f\n\n", CALIBRATION_FACTOR);

  Serial.println("Ready. Magnet passages will now be interpreted as half-revolutions.\n");
}

void loop() {
  static int   lastPrintedCount  = -1;
  static float lastPrintedMeters = -1.0f;

  if (pulseCount != lastPrintedCount) {
    // Convert pulses → full revolutions → calibrated line length
    float revolutions = pulseCount * PULSES_TO_REVOLUTIONS;
    float distance_m  = revolutions * CIRCUMFERENCE * CALIBRATION_FACTOR;
    float distance_ft = distance_m * METERS_TO_FEET;

    // Print only when length has changed meaningfully (avoids console spam)
    if (fabs(distance_m - lastPrintedMeters) > 0.01f) {
      Serial.printf("Pulses: %3d   |   Revolutions: %5.2f   |   Length: %6.2f m   (%6.1f ft)\n",
                    pulseCount, revolutions, distance_m, distance_ft);

      lastPrintedMeters = distance_m;
    }

    lastPrintedCount = pulseCount;
  }

  delay(50);   // Light delay → responsive yet low CPU usage
}
