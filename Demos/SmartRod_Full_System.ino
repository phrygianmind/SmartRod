#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BluetoothSerial.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// ==================== Bluetooth Configuration ====================
BluetoothSerial SerialBT;
unsigned long lastBtTxMs = 0;
const unsigned long BT_TX_INTERVAL = 50;

// ==================== State Machine ====================
// ARMED    : IMU active, waiting for cast onset
// CASTING  : IMU active, peak cast force tracked for power bar
// WAIT_BITE: Piezo monitored for bite detection after cast settles
// REELING  : Hall sensor active, distance measured while reeling in
enum RodState : uint8_t { ARMED = 0, CASTING = 1, WAIT_BITE = 2, REELING = 3 };
RodState state = ARMED;

// -------------------- Hardware Pins --------------------

// MPU6050 I2C
static const int MPU_SDA = 21;
static const int MPU_SCL = 22;
static const uint8_t MPU_ADDR = 0x68;

Adafruit_MPU6050 mpu;

// Hall-effect line counter
static const int HALL_PIN = 27;

// Piezo bite sensor
static const int PIEZO_PIN = 34;

// Active buzzer output
static const int BUZZER_PIN = 25;

// ESP32 DevKit onboard BOOT button (active LOW)
static const int BUTTON_PIN = 0;

// DS18B20 water temperature sensor
static const int ONE_WIRE_BUS = 4;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);

// Nokia 5110 (PCD8544)
static const int LCD5110_SCLK = 18;
static const int LCD5110_DIN  = 23;
static const int LCD5110_DC   = 14;
static const int LCD5110_CS   = 26;
static const int LCD5110_RST  = 33;

Adafruit_PCD8544 display(LCD5110_SCLK, LCD5110_DIN, LCD5110_DC, LCD5110_CS, LCD5110_RST);

// -------------------- Timing --------------------

const unsigned long lcdMs = 120;
unsigned long lastLcdMs = 0;

// DS18B20 timing
const unsigned long TEMP_SAMPLE_MS = 1000;
unsigned long lastTempMs = 0;
float waterTempC = DEVICE_DISCONNECTED_C;

// -------------------- Hall Distance --------------------

const float R_CORE        = 0.02413f;
const float R_FULL        = 0.03048f;
const float AVG_RADIUS    = (R_CORE + R_FULL) / 2.0f;
const float CIRCUMFERENCE = TWO_PI * AVG_RADIUS;

const float MAGNETS_PER_REVOLUTION = 2.0f;
const float PULSES_TO_REVOLUTIONS  = 1.0f / MAGNETS_PER_REVOLUTION;

const float CALIBRATION_FACTOR = 1.00f;

volatile int pulseCount = 0;
volatile unsigned long lastTriggerTime = 0;
volatile bool hallPulseFlag = false;

float savedCastDistance = 0.0f;

// -------------------- IMU Power Meter --------------------

const float MASS_KG = 0.05f;
const float MAX_FORCE_EXPECTED = 1.0f;

static float amagLP = 0.0f;
const float LP_ALPHA = 0.995f;

float forceNewtons = 0.0f;
float forceHold = 0.0f;
bool  forceUpdated = false;

const unsigned long HOLD_MS = 3000;
unsigned long holdUntilMs = 0;

const float HOLD_DECAY_PER_UPDATE = 0.96f;
const float HOLD_FLOOR = 0.01f;

float imuDynAccel = 0.0f;

// -------------------- Piezo Bite Detection --------------------

int piezoBaseline = 0;
float piezoAlpha = 0.02f;
int piezoSpike = 0;

// Retained for developer convenience, but bite now transitions into REELING.
static const bool AUTO_REARM_AFTER_BITE = false;

// HIGH, MED, LOW sensitivity thresholds
int BITE_THRESH[3] = { 500, 1500, 3000 };
uint8_t sensIdx = 1;

unsigned long biteLockoutUntil = 0;
const unsigned long BITE_LOCKOUT_MS = 600;

unsigned long biteBannerUntil = 0;
const unsigned long BITE_BANNER_MS = 5000;

// -------------------- Button / Sensitivity Toggle --------------------

bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastButtonEdgeMs = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 40;

// -------------------- Cast Detection Thresholds --------------------

const float CAST_START_DYN = 1.20f;
const float CAST_END_DYN   = 0.35f;
const unsigned long CAST_END_QUIET_MS = 450;
const unsigned long CAST_CONFIRM_MS = 40;

unsigned long castCandidateStartMs = 0;
unsigned long castQuietStartMs = 0;

// -------------------- Post-cast Settling --------------------

unsigned long biteEnableAtMs = 0;
const unsigned long POST_CAST_SETTLE_MS = 900;

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

// Hall pulse ISR with debounce.
// Pulse acceptance is deferred to loop() so counting can be state-gated safely.
void IRAM_ATTR hall_ISR() {
  unsigned long currentTime = millis();

  if (currentTime - lastTriggerTime > 200) {
    lastTriggerTime = currentTime;
    hallPulseFlag = true;
  }
}

// Active buzzer drive helpers.
static void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

static void buzzerOn() {
  digitalWrite(BUZZER_PIN, HIGH);
}

static void buzzerPulse(unsigned long onMs, unsigned long offMs) {
  buzzerOn();
  delay(onMs);
  buzzerOff();
  delay(offMs);
}

// Short blocking alert sequence used only on bite events.
static void playBiteAlert() {
  buzzerPulse(120, 80);
  buzzerPulse(120, 80);
  buzzerPulse(180, 0);
}

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

  biteEnableAtMs = now + 9999999UL;

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

  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Reset");
  display.display();
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

void setup() {
  Serial.begin(115200);
  SerialBT.begin("CyberFish_Rod");

  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hall_ISR, FALLING);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);

  display.begin();
  display.setContrast(55);
  display.setRotation(2);
  display.clearDisplay();
  display.display();

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  Wire.begin(MPU_SDA, MPU_SCL);
  Wire.setClock(400000);

  if (!mpu.begin(MPU_ADDR, &Wire)) {
    Serial.println("ERROR: MPU6050 init failed.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("MPU6050 FAIL");
    display.display();

    while (true) delay(100);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  waterTempSensor.begin();
  Serial.print("DS18B20 devices found: ");
  Serial.println(waterTempSensor.getDeviceCount());

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(200);
  int sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(PIEZO_PIN);
    delay(2);
  }
  piezoBaseline = sum / 50;

  waterTempSensor.requestTemperatures();
  waterTempC = waterTempSensor.getTempCByIndex(0);

  enterArmed(millis());
}

void loop() {
  unsigned long now = millis();

  // -------------------- Button Update --------------------
  updateButton(now);

  // -------------------- Water Temperature --------------------
  updateWaterTemperature(now);

  // -------------------- Hall Pulse Consume --------------------
  if (hallPulseFlag) {
    noInterrupts();
    hallPulseFlag = false;
    interrupts();

    if (state == REELING) {
      pulseCount++;
    }
  }

  // -------------------- IMU Read --------------------
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  {
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;

    float amag = sqrtf(ax * ax + ay * ay + az * az);

    if (amagLP == 0.0f) amagLP = amag;
    amagLP = LP_ALPHA * amagLP + (1.0f - LP_ALPHA) * amag;

    imuDynAccel = fabsf(amag - amagLP);

    forceNewtons = MASS_KG * imuDynAccel;
    forceUpdated = true;
  }

  // -------------------- State Machine --------------------
  updateStateMachine(now);

  // -------------------- Piezo Read + Bite Detect --------------------
  {
    int v = analogRead(PIEZO_PIN);

    if (piezoBaseline == 0) piezoBaseline = v;
    piezoBaseline = (int)((1.0f - piezoAlpha) * piezoBaseline + piezoAlpha * v);
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

  // -------------------- Peak Hold / Decay --------------------
  if (forceUpdated) {
    if (state == CASTING) {
      if (forceNewtons > forceHold) {
        forceHold = forceNewtons;
        holdUntilMs = now + HOLD_MS;
      }
    }
    forceUpdated = false;
  }

  if (now >= holdUntilMs) {
    forceHold *= HOLD_DECAY_PER_UPDATE;
    if (forceHold < HOLD_FLOOR) forceHold = 0.0f;
  }

  // -------------------- Distance --------------------
  float distance_m = savedCastDistance;

  if (state == REELING) {
    int localPulseCount;
    noInterrupts();
    localPulseCount = pulseCount;
    interrupts();

    float revolutions = localPulseCount * PULSES_TO_REVOLUTIONS;
    distance_m = revolutions * CIRCUMFERENCE * CALIBRATION_FACTOR;
    savedCastDistance = distance_m;
  }

  // -------------------- Display --------------------
  if (now - lastLcdMs >= lcdMs) {
    lastLcdMs = now;
    bool biteBanner = (now < biteBannerUntil);
    drawNokiaUI(savedCastDistance, forceHold, state, biteBanner, sensIdx, waterTempC);
  }

  // ==================== APP DATA TRANSMISSION ====================
  if (now - lastBtTxMs >= BT_TX_INTERVAL) {
    lastBtTxMs = now;

    if (SerialBT.hasClient()) {
      String biteStatus = (now < biteBannerUntil) ? "FISH" : "No";

      String packet = String(forceHold, 2) + "," +
                      biteStatus + "," +
                      stateLabel(state, (now < biteBannerUntil)) + "," +
                      sensLabel(sensIdx) + "," +
                      String(savedCastDistance, 1) + "," +
                      (waterTempC == DEVICE_DISCONNECTED_C ? String("NA") : String(waterTempC, 1));

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

  // -------------------- Serial Commands --------------------
  if (Serial.available()) {
    char ch = (char)Serial.read();
    handleCommand(ch);
  }

  // -------------------- Bluetooth Commands --------------------
  if (SerialBT.available()) {
    char ch = (char)SerialBT.read();
    handleCommand(ch);
  }
}
