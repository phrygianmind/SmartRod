#include <Arduino.h>
#include <math.h>

// --------------------------------------------------
// Piezo spike test for ESP32
// - Reads a piezo sensor on an ADC pin
// - Learns a moving baseline and noise floor
// - Turns the onboard LED on when a significant spike is detected
// - Prints raw data for Serial Plotter tuning
//
// Typical wiring:
//   Piezo +  -> GPIO34 through ~1k series resistor
//   Piezo -  -> GND
//   GPIO34   -> 1M resistor -> GND   (pulldown)
// --------------------------------------------------

// Hardware pins
static const int PIEZO_PIN = 34;   // ADC-only pin on ESP32, good for piezo input
static const int LED_PIN   = 2;    // Typical onboard LED pin on many ESP32 dev boards

// Detection tuning
static const float BASELINE_ALPHA = 0.995f;  // Higher = slower baseline movement
static const float NOISE_ALPHA    = 0.98f;   // Higher = smoother noise estimate
static const float NOISE_MULT     = 6.0f;    // Threshold = max(MIN_SPIKE, noise * multiplier)
static const float MIN_SPIKE      = 80.0f;   // Absolute minimum delta threshold
static const float NOISE_CAP      = 200.0f;  // Prevent very large hits from corrupting noise estimate

// Timing
static const unsigned long LED_HOLD_MS  = 250;
static const unsigned long COOLDOWN_MS  = 120;
static const unsigned long PRINT_MS     = 10;

// State
static bool initialized = false;
static float baseline   = 0.0f;
static float noiseFloor = 0.0f;

static unsigned long ledOffAt     = 0;
static unsigned long lastTrigger  = 0;
static unsigned long lastPrintMs  = 0;

void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  analogReadResolution(12);                     // 0..4095
  analogSetPinAttenuation(PIEZO_PIN, ADC_11db); // Best full-scale range for ESP32 ADC

  Serial.println();
  Serial.println("Piezo spike test started.");
  Serial.println("raw,baseline,delta,threshold,event");
}

void loop() {
  const unsigned long now = millis();
  const int raw = analogRead(PIEZO_PIN);

  if (!initialized) {
    baseline = raw;
    noiseFloor = 0.0f;
    initialized = true;
  }

  // Difference between current sample and learned baseline
  float delta = fabsf((float)raw - baseline);

  // During cooldown, avoid re-trigger chatter and avoid learning the event into baseline
  const bool inCooldown = (now - lastTrigger) < COOLDOWN_MS;

  // Update baseline only when not inside the post-hit cooldown
  if (!inCooldown) {
    baseline = BASELINE_ALPHA * baseline + (1.0f - BASELINE_ALPHA) * raw;
  }

  // Update learned noise floor from smaller, non-event movement only
  if (!inCooldown) {
    float noiseSample = delta;
    if (noiseSample > NOISE_CAP) noiseSample = NOISE_CAP;
    noiseFloor = NOISE_ALPHA * noiseFloor + (1.0f - NOISE_ALPHA) * noiseSample;
  }

  // Adaptive threshold
  float threshold = noiseFloor * NOISE_MULT;
  if (threshold < MIN_SPIKE) threshold = MIN_SPIKE;

  // Event detection
  bool event = false;
  if (!inCooldown && delta > threshold) {
    event = true;
    lastTrigger = now;
    ledOffAt = now + LED_HOLD_MS;
    setLed(true);
  }

  // LED hold timer
  if ((long)(now - ledOffAt) >= 0) {
    setLed(false);
  }

  // Serial Plotter / Serial Monitor output
  if (now - lastPrintMs >= PRINT_MS) {
    lastPrintMs = now;

    Serial.print(raw);
    Serial.print(",");
    Serial.print((int)baseline);
    Serial.print(",");
    Serial.print((int)delta);
    Serial.print(",");
    Serial.print((int)threshold);
    Serial.print(",");
    Serial.println(event ? 1 : 0);
  }
}