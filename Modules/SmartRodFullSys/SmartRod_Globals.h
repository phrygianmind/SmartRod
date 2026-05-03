#ifndef SMARTROD_GLOBALS_H
#define SMARTROD_GLOBALS_H

#include "SmartRod_Config.h"

BluetoothSerial SerialBT;
Adafruit_MPU6050 mpu;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);

Adafruit_PCD8544 display(LCD5110_SCLK, LCD5110_DIN, LCD5110_DC, LCD5110_CS, LCD5110_RST);

unsigned long lastBtTxMs = 0;
RodState state = ARMED;

unsigned long lastLcdMs = 0;
unsigned long lastTempMs = 0;
float waterTempC = DEVICE_DISCONNECTED_C;

volatile int pulseCount = 0;
volatile unsigned long lastTriggerTime = 0;
volatile bool hallPulseFlag = false;

float savedCastDistance = 0.0f;

static float amagLP = 0.0f;
float forceNewtons = 0.0f;
float forceHold = 0.0f;
bool  forceUpdated = false;
unsigned long holdUntilMs = 0;
float imuDynAccel = 0.0f;

int piezoBaseline = 0;
int piezoSpike = 0;
uint8_t sensIdx = 1;

unsigned long biteLockoutUntil = 0;
unsigned long biteBannerUntil = 0;

bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastButtonEdgeMs = 0;

unsigned long castCandidateStartMs = 0;
unsigned long castQuietStartMs = 0;

unsigned long biteEnableAtMs = 0;

#endif
