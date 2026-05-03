#include "SmartRod_Config.h"
#include "SmartRod_Globals.h"
#include "SmartRod_Labels.h"
#include "SmartRod_Buzzer.h"
#include "SmartRod_Button.h"
#include "SmartRod_Temperature.h"
#include "SmartRod_Hall.h"
#include "SmartRod_IMU.h"
#include "SmartRod_StateMachine.h"
#include "SmartRod_Piezo.h"
#include "SmartRod_Display.h"
#include "SmartRod_Distance.h"
#include "SmartRod_Bluetooth.h"
#include "SmartRod_Commands.h"

void setup() {
  Serial.begin(115200);
  SerialBT.begin(BT_DEVICE_NAME);

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
    showMpuFailScreen();

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
  calibratePiezoBaseline();

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
  consumeHallPulse();

  // -------------------- IMU Read --------------------
  updateImu();

  // -------------------- State Machine --------------------
  updateStateMachine(now);

  // -------------------- Piezo Read + Bite Detect --------------------
  updatePiezoBite(now);

  // -------------------- Peak Hold / Decay --------------------
  updatePeakHold(now);

  // -------------------- Distance --------------------
  float distance_m = updateDistance();

  // -------------------- Display --------------------
  if (now - lastLcdMs >= lcdMs) {
    lastLcdMs = now;
    bool biteBanner = (now < biteBannerUntil);
    drawNokiaUI(distance_m, forceHold, state, biteBanner, sensIdx, waterTempC);
  }

  // ==================== APP DATA TRANSMISSION ====================
  updateBluetoothTelemetry(now);

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
