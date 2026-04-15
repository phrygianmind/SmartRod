#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <LiquidCrystal.h>
#include <math.h>
#include "Adafruit_BNO08x_RVC.h"
#include "BluetoothSerial.h"

BluetoothSerial ESP_BT;
// TCS34725 on I2C
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;

// BNO085 in UART-RVC mode (BNO SDA -> ESP32 RX)
static const int BNO_UART_RX = 19;
static const int BNO_UART_TX = -1;
static const uint32_t BNO_UART_BAUD = 115200;

HardwareSerial BNO_Serial(2);
Adafruit_BNO08x_RVC rvc;

Adafruit_TCS34725 tcs(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// LCD1602 (4-bit parallel)
const int LCD_RS = 14;
const int LCD_E  = 27;
const int LCD_D4 = 26;
const int LCD_D5 = 25;
const int LCD_D6 = 33;
const int LCD_D7 = 32;

// constructor is (rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Timers
const unsigned long sampleMs  = 70;
const unsigned long lcdMs     = 120;
const unsigned long lockoutMs = 120;

unsigned long lastSampleMs = 0;
unsigned long lastLcdMs    = 0;
unsigned long lastCountMs  = 0;

// Spool / line geometry (meters)
const float R_core_m  = 0.02413f;
const float R_full_m  = 0.03048f;
const float spoolW_m  = 0.02413f;
const float lineDia_m = 0.00030f;

float calib = 1.00f;

// Constant Approximate Radius (tuned for ~16 m average retrieve) 
// Midpoint is ~0.0273 m; for ~16 m (≈1.2 layers) a slightly higher average works well.
const float R_CONST_m = 0.0276f;   // <-- 16 m avg effective radius

// Distance model state
int   count   = 0;
float L_out_m = 0.0f;

// Power meter
const float MASS_KG = 0.05f;
const float MAX_FORCE_EXPECTED = 1.0f;

static float amagLP = 0.0f;
const float LP_ALPHA = 0.995f;

float forceNewtons = 0.0f;
float forceHold = 0.0f;
bool  forceUpdated = false;

// Hold then decay
const unsigned long HOLD_MS = 3000;     // freeze bar for this long after a peak
unsigned long holdUntilMs = 0;

const float HOLD_DECAY_PER_UPDATE = 0.96f; // decay speed (tune if needed)
const float HOLD_FLOOR = 0.01f;

// Red mark detection
bool isRedMark(uint16_t r, uint16_t g, uint16_t b) {
  return (r > (uint16_t)(g * 1.5f)) && (r > (uint16_t)(b * 1.5f));
}

// Constant-radius distance per revolution
void onRevolution() {
  float dL = calib * (2.0f * (float)M_PI * R_CONST_m);
  L_out_m += dL;
}

void resetModel() {
  count = 0;
  L_out_m = 0.0f;
  lastCountMs = millis();

  forceNewtons = 0.0f;
  forceHold = 0.0f;
  amagLP = 0.0f;
  forceUpdated = false;
  holdUntilMs = 0;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Reset");
  lcd.setCursor(0, 1); lcd.print("...");
}

void setup() {
  Serial.begin(115200);
  ESP_BT.begin("CYberfish_Rod");
  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Smart Rod");
  lcd.setCursor(0, 1); lcd.print("Init...");

  if (!tcs.begin()) {
    Serial.println("ERROR: TCS34725 not found.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("TCS34725");
    lcd.setCursor(0, 1); lcd.print("NOT FOUND");
    while (true) delay(100);
  }

  BNO_Serial.begin(BNO_UART_BAUD, SERIAL_8N1, BNO_UART_RX, BNO_UART_TX);
  delay(1200);

  if (!rvc.begin(&BNO_Serial)) {
    Serial.println("ERROR: BNO085 UART-RVC init failed.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("BNO085 UART");
    lcd.setCursor(0, 1); lcd.print("INIT FAIL");
    while (true) delay(100);
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Ready");
  lcd.setCursor(0, 1); lcd.print("Counting...");
  delay(600);
  lcd.clear();

  Serial.println("TCS distance + BNO power (UART-RVC). Constant R mode (~16m avg). Send 'r' to reset.");
}

void loop() {
  unsigned long now = millis();

  // Sample color sensor and count revolutions
  if (now - lastSampleMs >= sampleMs) {
    lastSampleMs = now;

    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    if (isRedMark(r, g, b)) {
      if (now - lastCountMs >= lockoutMs) {
        lastCountMs = now;
        count++;
        onRevolution();
      }
    }
  }
  // LCD update
  if (now - lastLcdMs >= lcdMs) {
    lastLcdMs = now;

    // --- PASTE BLUETOOTH CODE HERE ---
    ESP_BT.print(forceHold, 2);    // Sends Force to Label 1
    ESP_BT.print(",");             // Comma for the "Split" block
    ESP_BT.print(L_out_m, 1);      // Sends Distance to Label 2
    ESP_BT.print(",");             // Comma for the "Split" block
    
    if (forceHold > 0.45) {        // If pull is strong
      ESP_BT.println("BITE!");     // Sends BITE to Label 3
    } else {
      ESP_BT.println("Scanning");  // Sends Scanning to Label 3
    }
    // ---------------------------------

    if (forceUpdated) {
      // ... rest of your existing LCD logic

  // Drain UART packets and compute force from the most recent one
  {
    BNO08x_RVC_Data data;
    bool gotPacket = false;

    while (rvc.read(&data)) {
      gotPacket = true;
    }

    if (gotPacket) {
      float ax = data.x_accel;
      float ay = data.y_accel;
      float az = data.z_accel;

      float amag = sqrtf(ax * ax + ay * ay + az * az);

      if (amagLP == 0.0f) amagLP = amag;
      amagLP = LP_ALPHA * amagLP + (1.0f - LP_ALPHA) * amag;

      float dynAccel = fabsf(amag - amagLP);
      forceNewtons = MASS_KG * dynAccel;
      forceUpdated = true;
    }
  }

  // LCD update
  if (now - lastLcdMs >= lcdMs) {
    lastLcdMs = now;

    if (forceUpdated) {
      if (forceNewtons > forceHold) {
        forceHold = forceNewtons;
        holdUntilMs = now + HOLD_MS;
      }
      forceUpdated = false;
    }

    if (now >= holdUntilMs) {
      forceHold *= HOLD_DECAY_PER_UPDATE;
      if (forceHold < HOLD_FLOOR) forceHold = 0.0f;
    }

    lcd.setCursor(0, 0);
    lcd.print("Dist:");
    lcd.print(L_out_m, 1);
    lcd.print("m      ");

    lcd.setCursor(0, 1);
    lcd.print("PWR:");

    float clamped = forceHold;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > MAX_FORCE_EXPECTED) clamped = MAX_FORCE_EXPECTED;

    int barLen = (int)floorf((clamped / MAX_FORCE_EXPECTED) * 12.0f);
    if (barLen < 0) barLen = 0;
    if (barLen > 12) barLen = 12;

    for (int i = 0; i < 12; i++) {
      lcd.print(i < barLen ? (char)255 : ' ');
    }
  }

  // Serial reset
  if (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == 'r' || ch == 'R') {
      resetModel();
      Serial.println("Reset.");
    }
  }
}
