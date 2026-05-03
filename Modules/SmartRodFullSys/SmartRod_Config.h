#ifndef SMARTROD_CONFIG_H
#define SMARTROD_CONFIG_H

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

#include <stdio.h>
#include <string.h>

// ==================== Bluetooth Configuration ====================
static const char BT_DEVICE_NAME[] = "CyberFish_Rod";
static const unsigned long BT_TX_INTERVAL = 50;

// ==================== State Machine ====================
// ARMED    : IMU active, waiting for cast onset
// CASTING  : IMU active, peak cast force tracked for power bar
// WAIT_BITE: Piezo monitored for bite detection after cast settles
// REELING  : Hall sensor active, distance measured while reeling in
enum RodState : uint8_t { ARMED = 0, CASTING = 1, WAIT_BITE = 2, REELING = 3 };

// -------------------- Hardware Pins --------------------

// MPU6050 I2C
static const int MPU_SDA = 21;
static const int MPU_SCL = 22;
static const uint8_t MPU_ADDR = 0x68;

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

// Nokia 5110 (PCD8544)
static const int LCD5110_SCLK = 18;
static const int LCD5110_DIN  = 23;
static const int LCD5110_DC   = 14;
static const int LCD5110_CS   = 26;
static const int LCD5110_RST  = 33;

// -------------------- Timing --------------------

static const unsigned long lcdMs = 120;

// DS18B20 timing
static const unsigned long TEMP_SAMPLE_MS = 1000;

// -------------------- Hall Distance --------------------

static const float R_CORE        = 0.02413f;
static const float R_FULL        = 0.03048f;
static const float AVG_RADIUS    = (R_CORE + R_FULL) / 2.0f;
static const float CIRCUMFERENCE = TWO_PI * AVG_RADIUS;

static const float MAGNETS_PER_REVOLUTION = 2.0f;
static const float PULSES_TO_REVOLUTIONS  = 1.0f / MAGNETS_PER_REVOLUTION;

static const float CALIBRATION_FACTOR = 1.00f;

// -------------------- IMU Power Meter --------------------

static const float MASS_KG = 0.05f;
static const float MAX_FORCE_EXPECTED = 1.0f;

static const float LP_ALPHA = 0.995f;

static const unsigned long HOLD_MS = 3000;

static const float HOLD_DECAY_PER_UPDATE = 0.96f;
static const float HOLD_FLOOR = 0.01f;

// -------------------- Piezo Bite Detection --------------------

static const float PIEZO_ALPHA = 0.02f;

// Retained for developer convenience, but bite now transitions into REELING.
static const bool AUTO_REARM_AFTER_BITE = false;

// HIGH, MED, LOW sensitivity thresholds
static const int BITE_THRESH[3] = { 500, 1500, 3000 };

static const unsigned long BITE_LOCKOUT_MS = 600;
static const unsigned long BITE_BANNER_MS = 5000;

// -------------------- Button / Sensitivity Toggle --------------------

static const unsigned long BUTTON_DEBOUNCE_MS = 40;

// -------------------- Cast Detection Thresholds --------------------

static const float CAST_START_DYN = 1.20f;
static const float CAST_END_DYN   = 0.35f;
static const unsigned long CAST_END_QUIET_MS = 450;
static const unsigned long CAST_CONFIRM_MS = 40;

// -------------------- Post-cast Settling --------------------

static const unsigned long POST_CAST_SETTLE_MS = 900;
static const unsigned long BITE_DISABLED_DURING_CAST_MS = 9999999UL;

#endif
