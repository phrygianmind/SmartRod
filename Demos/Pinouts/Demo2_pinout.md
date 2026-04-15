# Smart Rod v2 (Demo 2) — Full Pinout (ESP32 DevKit V1)

Demo 2 changes vs v1:

* Nokia 5110 (PCD8544) replaces the LCD1602
* Hall-effect sensor replaces the TCS34725 color sensor
* BNO085 (UART-RVC) still used for IMU bite-detection logic
* Piezo sensor used for bite detection
* Active buzzer added for bite alerts

---

## ESP32 DevKit V1

* **3V3**: powers Nokia 5110 + BNO085 + Hall sensor
* **GND**: common ground for all modules
* **5V / VIN**: not required for Demo 2 display (Nokia 5110 is 3.3V)

---

## BNO085 IMU (UART-RVC)

> UART-RVC uses the BNO’s **SDA pin as UART TX output**.

* **VIN** → ESP32 **3V3**
* **GND** → ESP32 **GND**
* **SDA (UART OUT)** → ESP32 **GPIO19** (UART2 RX)
* **P0** → ESP32 **3V3**
* **SCL** → **NC** (not used)
* ESP32 UART TX → **NC** (not used)

---

## Nokia 5110 LCD (PCD8544) — Demo 2 Display

### Nokia 5110 pin labels

Common breakout labels: **RST, CE (CS), DC, DIN, CLK, VCC, BL, GND**

### Power / backlight

* **VCC** → ESP32 **3V3**
* **GND** → ESP32 **GND**
* **BL** → ESP32 **3V3**

### Control + data (matches standalone test code)

Uses ESP32 hardware SPI lines + control pins:

* **CLK** → ESP32 **GPIO18** *(SCK)*
* **DIN** → ESP32 **GPIO23** *(MOSI)*
* **DC**  → ESP32 **GPIO14**
* **CE/CS** → ESP32 **GPIO33**
* **RST** → ESP32 **GPIO26**

*(Nokia 5110 does not use MISO.)*

---

## Hall-Effect Sensor (Spool Revolution Sensor)

Replaces the TCS34725. Used to count spool rotations for Demo 2.

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

## Notes

* Nokia 5110 is **3.3V logic** — do **not** power it from 5V.
* **GPIO34** is input-only, which is suitable for the piezo analog input.
* Hall sensor is configured as an interrupt input on **GPIO27**.
* Nokia **CE/CS** moved to **GPIO33**.

