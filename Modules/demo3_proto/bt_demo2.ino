#include <Arduino.h>
#include <math.h>
#include "Adafruit_BNO08x_RVC.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BluetoothSerial.h" 

// ==================== Bluetooth Configuration ====================
BluetoothSerial SerialBT; 
unsigned long lastBtTxMs = 0;
// Set to 50ms (20Hz). This is fast enough for real-time bite detection 
// and matches the App Inventor clock to prevent buffer lag.
const unsigned long BT_TX_INTERVAL = 50; 

// ==================== State Machine ====================
enum RodState : uint8_t { ARMED = 0, CASTING = 1, WAIT_BITE = 2 };
RodState state = ARMED;

// -------------------- Hardware Pins --------------------
static const int BNO_UART_RX = 19;
static const int BNO_UART_TX = -1;
static const uint32_t BNO_UART_BAUD = 115200;

HardwareSerial BNO_Serial(2);
Adafruit_BNO08x_RVC rvc;

static const int HALL_PIN = 27;
static const int PIEZO_PIN = 34;
static const int BUZZER_PIN = 25;
static const int BUTTON_PIN = 0;

static const int LCD5110_SCLK = 18;
static const int LCD5110_DIN  = 23;
static const int LCD5110_DC   = 14;
static const int LCD5110_CS   = 33;
static const int LCD5110_RST  = 26;

Adafruit_PCD8544 display(LCD5110_SCLK, LCD5110_DIN, LCD5110_DC, LCD5110_CS, LCD5110_RST);

// -------------------- Timing --------------------
const unsigned long lcdMs = 120;
unsigned long lastLcdMs = 0;

// -------------------- Hall Distance --------------------
const float R_CORE        = 0.02413f;
const float R_FULL        = 0.03048f;
const float AVG_RADIUS    = (R_CORE + R_FULL) / 2.0f;
const float CIRCUMFERENCE = TWO_PI * AVG_RADIUS;
const float MAGNETS_PER_REVOLUTION = 2.0f;
const float PULSES_TO_REVOLUTIONS  = 1.0f / MAGNETS_PER_REVOLUTION;
const float CALIBRATION_FACTOR = 1.00f;
const float METERS_TO_FEET     = 3.28084f;

volatile int pulseCount = 0;
volatile unsigned long lastTriggerTime = 0;

// THIS IS THE NEW VARIABLE TO LOCK YOUR DISTANCE
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
static const bool AUTO_REARM_AFTER_BITE = true; 
int BITE_THRESH[3] = { 500, 1500, 3000 };
uint8_t sensIdx = 1;
unsigned long biteLockoutUntil = 0;
const unsigned long BITE_LOCKOUT_MS = 600;
unsigned long biteBannerUntil = 0;
const unsigned long BITE_BANNER_MS = 5000;

// -------------------- Button --------------------
bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastButtonEdgeMs = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 40;

// -------------------- Cast Thresholds --------------------
const float CAST_START_DYN = 1.20f;
const float CAST_END_DYN   = 0.35f;
const unsigned long CAST_END_QUIET_MS = 450;
const unsigned long CAST_CONFIRM_MS = 40;
unsigned long castCandidateStartMs = 0;
unsigned long castQuietStartMs = 0;
unsigned long biteEnableAtMs = 0;
const unsigned long POST_CAST_SETTLE_MS = 900;

// -------------------- Helpers --------------------
static const char* stateLabel(RodState st, bool biteBanner) {
  if (biteBanner) return "BITE";
  switch (st) {
    case ARMED:     return "ARM";
    case CASTING:   return "CAST";
    case WAIT_BITE: return "WAIT";
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

void IRAM_ATTR hall_ISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastTriggerTime > 200) {
    pulseCount++;
    lastTriggerTime = currentTime;
  }
}

static void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); }
static void buzzerOn()  { digitalWrite(BUZZER_PIN, HIGH); }
static void buzzerPulse(unsigned long onMs, unsigned long offMs) {
  buzzerOn(); delay(onMs); buzzerOff(); delay(offMs);
}

static void playBiteAlert() {
  buzzerPulse(120, 80); buzzerPulse(120, 80); buzzerPulse(180, 0);
}

static void cycleSensitivity() {
  sensIdx++;
  if (sensIdx > 2) sensIdx = 0;
  Serial.print("Sensitivity: ");
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
      if (buttonStableState == LOW) cycleSensitivity();
    }
  }
}

static void enterArmed(unsigned long now) {
  state = ARMED; castCandidateStartMs = 0; castQuietStartMs = 0;
  biteBannerUntil = 0; biteLockoutUntil = 0;
  biteEnableAtMs = now + 300;
}

static void enterCasting(unsigned long now) {
  state = CASTING; forceNewtons = 0.0f; forceHold = 0.0f;
  forceUpdated = false; holdUntilMs = 0;
  biteEnableAtMs = now + 9999999UL;

  // RESET SPOOL COUNTER EXACTLY WHEN CAST STARTS
  noInterrupts(); 
  pulseCount = 0; 
  interrupts();
  savedCastDistance = 0.0f; // Reset display to 0 for the new cast
}

static void enterWaitBite(unsigned long now) {
  state = WAIT_BITE;
  biteEnableAtMs = now + POST_CAST_SETTLE_MS;
}

static void updateStateMachine(unsigned long now) {
  if (state == ARMED) {
    if (imuDynAccel >= CAST_START_DYN) {
      if (castCandidateStartMs == 0) castCandidateStartMs = now;
      if (now - castCandidateStartMs >= CAST_CONFIRM_MS) enterCasting(now);
    } else castCandidateStartMs = 0;
  } else if (state == CASTING) {
    if (imuDynAccel <= CAST_END_DYN) {
      if (castQuietStartMs == 0) castQuietStartMs = now;
      if (now - castQuietStartMs >= CAST_END_QUIET_MS) enterWaitBite(now);
    } else castQuietStartMs = 0;
  }
}

static void drawNokiaUI(float distance_m, float distance_ft, float forceHeldN,
                        RodState st, bool biteBanner, uint8_t sensitivity) {
  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.print("State: "); display.print(stateLabel(st, biteBanner));
  display.setCursor(0, 10);
  display.print("Line: "); display.print(distance_m, 1); display.print("m");
  display.setCursor(0, 20);
  display.print(distance_ft, 1); display.print("ft");
  display.setCursor(0, 30);
  display.print("Sens: "); display.print(sensLabel(sensitivity));

  const int barX = 0; const int barY = 40; const int barW = 84; const int barH = 8;
  display.drawRect(barX, barY, barW, barH, BLACK);
  float clamped = (forceHeldN > MAX_FORCE_EXPECTED) ? MAX_FORCE_EXPECTED : (forceHeldN < 0 ? 0 : forceHeldN);
  int fillW = (int)((clamped / MAX_FORCE_EXPECTED) * (barW - 2));
  if (fillW > 0) display.fillRect(barX + 1, barY + 1, fillW, barH - 2, BLACK);
  display.display();
}

static void resetModel() {
  noInterrupts(); pulseCount = 0; interrupts();
  forceHold = 0.0f; amagLP = 0.0f;
  enterArmed(millis());
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("CyberFish_Rod"); 

  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hall_ISR, FALLING);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT); buzzerOff();

  display.begin();
  display.setContrast(55);
  display.setRotation(2);

  BNO_Serial.begin(BNO_UART_BAUD, SERIAL_8N1, BNO_UART_RX, BNO_UART_TX);
  delay(1200);
  if (!rvc.begin(&BNO_Serial)) {
    while (true) delay(100);
  }

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  int sum = 0;
  for (int i = 0; i < 50; i++) { sum += analogRead(PIEZO_PIN); delay(2); }
  piezoBaseline = sum / 50;
  enterArmed(millis());
}

void loop() {
  unsigned long now = millis();
  updateButton(now);

  // IMU acceleration tracking for power bar
  if (state != WAIT_BITE) {
    BNO08x_RVC_Data data;
    if (rvc.read(&data)) {
      float amag = sqrtf(data.x_accel * data.x_accel + data.y_accel * data.y_accel + data.z_accel * data.z_accel);
      if (amagLP == 0.0f) amagLP = amag;
      amagLP = LP_ALPHA * amagLP + (1.0f - LP_ALPHA) * amag;
      imuDynAccel = fabsf(amag - amagLP);
      forceNewtons = MASS_KG * imuDynAccel;
      forceUpdated = true;
    }
  }

  updateStateMachine(now);

  // Piezo bite detection
  int v = analogRead(PIEZO_PIN);
  piezoBaseline = (int)((1.0f - piezoAlpha) * piezoBaseline + piezoAlpha * v);
  piezoSpike = abs(v - piezoBaseline);

  if (state == WAIT_BITE && now >= biteEnableAtMs && now >= biteLockoutUntil) {
    if (piezoSpike > BITE_THRESH[sensIdx]) {
      biteBannerUntil = now + BITE_BANNER_MS;
      biteLockoutUntil = now + BITE_LOCKOUT_MS;
      playBiteAlert();
      if (AUTO_REARM_AFTER_BITE) enterArmed(millis());
    }
  }

  // Peak hold logic
  if (forceUpdated) {
    if (state == CASTING && forceNewtons > forceHold) {
      forceHold = forceNewtons;
      holdUntilMs = now + HOLD_MS;
    }
    forceUpdated = false;
  }
  if (now >= holdUntilMs) {
    forceHold *= HOLD_DECAY_PER_UPDATE;
    if (forceHold < HOLD_FLOOR) forceHold = 0.0f;
  }

  // Distance calculation
  int localPulses; noInterrupts(); localPulses = pulseCount; interrupts();
  float dist_m = localPulses * PULSES_TO_REVOLUTIONS * CIRCUMFERENCE * CALIBRATION_FACTOR;
  
  // LOCK THE DISTANCE: Only update the saved number while actively casting
  if (state == CASTING) {
    savedCastDistance = dist_m;
  }

  // LCD Update (Using the locked distance so the screen matches the app)
  if (now - lastLcdMs >= lcdMs) {
    lastLcdMs = now;
    drawNokiaUI(savedCastDistance, savedCastDistance * METERS_TO_FEET, forceHold, state, (now < biteBannerUntil), sensIdx);
  }

  // ==================== APP DATA TRANSMISSION ====================
  if (now - lastBtTxMs >= BT_TX_INTERVAL) {
    lastBtTxMs = now;

    if (SerialBT.hasClient()) {
      
      // 1. The dynamic Bite String: Shows "FISH" when the piezo triggers, otherwise "No"
      String biteStatus = (now < biteBannerUntil) ? "FISH" : "No";

      // 2. The cleanly formatted 5-part data package
      // Index 1: Force in Newtons 
      // Index 2: Bite Status (FISH / No)
      // Index 3: Rod State (ARM / CAST / WAIT)
      // Index 4: Sensitivity (HIGH / MED / LOW)
      // Index 5: LOCKED Distance in Meters
      String packet = String(forceHold, 2) + "," + 
                      biteStatus + "," + 
                      stateLabel(state, (now < biteBannerUntil)) + "," +
                      sensLabel(sensIdx) + "," +
                      String(savedCastDistance, 1);

      SerialBT.println(packet); 
      
      // Feedback for debugging in Serial Monitor
      Serial.print("[BT SENDING]: ");
      Serial.println(packet);
    } 
    else {
      // Notification if phone is not connected
      static unsigned long lastWait = 0;
      if (now - lastWait > 5000) {
        Serial.println("[BT STATUS]: Waiting for CyberFish app...");
        lastWait = now;
      }
    }
  }

  // Serial Monitor controls
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'r' || ch == 'R') resetModel();
  }
}
