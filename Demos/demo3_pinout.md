# Smart Rod v3 (Demo 3) — Full Pinout (ESP32 DevKit V1)

Demo 3 changes vs v2:

* MPU6050 replaces the BNO085 for IMU cast-detection / power-bar logic
* Nokia 5110 (PCD8544) still used for display
* Hall-effect sensor still used for spool rotation counting
* Piezo sensor still used for bite detection
* Active buzzer still used for bite alerts
* Bluetooth Serial added for app communication

---

## ESP32 DevKit V1

* **3V3**: powers Nokia 5110 + MPU6050 + Hall sensor
* **GND**: common ground for all modules
* **5V / VIN**: not required for Demo 3 display/IMU setup

---

## MPU6050 IMU (I2C)

* **VCC** → ESP32 **3V3**
* **GND** → ESP32 **GND**
* **SDA** → ESP32 **GPIO21**
* **SCL** → ESP32 **GPIO22**
* **AD0** → **GND** *(default I2C address 0x68)*

---

## Nokia 5110 LCD (PCD8544) — Demo 3 Display

### Nokia 5110 pin labels

Common breakout labels: **RST, CE (CS), DC, DIN, CLK, VCC, BL, GND**

### Power / backlight

* **VCC** → ESP32 **3V3**
* **GND** → ESP32 **GND**
* **BL** → ESP32 **3V3**

### Control + data

Uses ESP32 hardware SPI lines + control pins:

* **CLK** → ESP32 **GPIO18** *(SCK)*
* **DIN** → ESP32 **GPIO23** *(MOSI)*
* **DC**  → ESP32 **GPIO14**
* **CE/CS** → ESP32 **GPIO33**
* **RST** → ESP32 **GPIO26**

*(Nokia 5110 does not use MISO.)*

---

## Hall-Effect Sensor (Spool Revolution Sensor)

Used to count spool rotations.

### Typical 3-pin hall module wiring (A3144-style)

* **VCC** → ESP32 **3V3**
* **GND** → ESP32 **GND**
* **OUT** → ESP32 **GPIO27**

---

## Piezo Sensor (Bite Detection)

Used for bite detection.

* **Signal** → ESP32 **GPIO34**
* Other piezo connection → **GND / appropriate conditioning circuit ground**

---

## Active Buzzer

Used for bite alerts.

* **SIG** → ESP32 **GPIO25**
* **GND** → ESP32 **GND**

---

## Bluetooth / App Communication

Uses ESP32 built-in Classic Bluetooth.

* No extra module required
* Device name in code: **CyberFish_Rod**

---

## Notes

* MPU6050 uses **I2C** instead of the Demo 2 BNO085 UART-RVC setup.
* Nokia 5110 is **3.3V logic** — do **not** power it from 5V.
* **GPIO34** is input-only, which is suitable for the piezo analog input.
* Hall sensor is configured as an interrupt input on **GPIO27**.
* ESP32 **BOOT button (GPIO0)** is used in code for sensitivity toggle.
