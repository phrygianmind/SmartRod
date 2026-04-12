# SmartRod Demo 3 Pinout README

This pinout reflects the current **Demo 3 codebase** using:
- **ESP32 DevKit**
- **MPU6050** over **I2C**
- **A3144 Hall-effect sensor**
- **Piezo sensor** for bite detection
- **Active buzzer**
- **Nokia 5110 LCD**
- **BluetoothSerial** for app telemetry

---

## ESP32 Pin Map

| Subsystem | Signal | ESP32 Pin | Notes |
|---|---|---:|---|
| MPU6050 | SDA | GPIO 21 | I2C data |
| MPU6050 | SCL | GPIO 22 | I2C clock |
| Hall sensor (A3144) | OUT | GPIO 27 | Interrupt input, `INPUT_PULLUP`, falling-edge trigger |
| Piezo sensor | Signal | GPIO 34 | ADC input only |
| Active buzzer | IN | GPIO 25 | Digital output |
| Sensitivity button | BOOT button | GPIO 0 | Active LOW, internal pull-up |
| Nokia 5110 LCD | SCLK / CLK | GPIO 18 | SPI clock |
| Nokia 5110 LCD | DIN / MOSI | GPIO 23 | SPI data |
| Nokia 5110 LCD | DC | GPIO 14 | Data/command select |
| Nokia 5110 LCD | CS | GPIO 33 | Chip select |
| Nokia 5110 LCD | RST | GPIO 26 | Display reset |
| Bluetooth app link | Classic Bluetooth | Internal | `BluetoothSerial`, no external UART wiring |

---

## 1. MPU6050 Wiring

The code uses the MPU6050 as the IMU for:
- cast detection
- power / force bar estimation
- motion tracking for the state machine

### MPU6050 -> ESP32

| MPU6050 Pin | Connect To | Notes |
|---|---|---|
| VCC | 3.3V | Recommended for ESP32 logic compatibility |
| GND | GND | Common ground |
| SDA | GPIO 21 | I2C data |
| SCL | GPIO 22 | I2C clock |
| AD0 | GND | Sets I2C address to `0x68` |
| INT | Not required | Unused in current code |
| XDA / XCL | Not required | Unused |
|

### Code reference

```cpp
static const int MPU_SDA = 21;
static const int MPU_SCL = 22;
static const uint8_t MPU_ADDR = 0x68;
```

If AD0 is tied HIGH instead, change the address to `0x69` in code.

---

## 2. A3144 Hall-Effect Sensor Wiring

The hall sensor is used to count spool magnet passes for line-distance tracking.

### A3144 -> ESP32

| A3144 Pin | Connect To | Notes |
|---|---|---|
| VCC | 3.3V or module-compatible VCC | Verify the exact breakout/module being used |
| GND | GND | Common ground |
| OUT | GPIO 27 | Uses ESP32 internal pull-up |

### Code behavior

The hall input is configured as:

```cpp
pinMode(HALL_PIN, INPUT_PULLUP);
attachInterrupt(digitalPinToInterrupt(HALL_PIN), hall_ISR, FALLING);
```

That means the code expects:
- the signal to idle HIGH
- the sensor to pull LOW on detection
- counting on the falling edge

### Important note

In the current integrated code, hall pulses increment `pulseCount`, but the display distance is intentionally **state-gated** through `savedCastDistance`. If live hall counting appears correct in a standalone test but not on the LCD, check the state logic around when `savedCastDistance` is allowed to update.

---

## 3. Piezo Sensor Wiring

The piezo sensor is used only for bite detection in `WAIT_BITE` state.

### Piezo -> ESP32

| Piezo Lead | Connect To | Notes |
|---|---|---|
| Signal lead | GPIO 34 | Analog input |
| Other lead | GND | Typical reference side |

### Code reference

```cpp
static const int PIEZO_PIN = 34;
```

GPIO 34 is input-only, which is appropriate for analog sensing.

---

## 4. Active Buzzer Wiring

The buzzer is used for bite alerts.

### Buzzer -> ESP32

| Buzzer Pin | Connect To | Notes |
|---|---|---|
| IN / + | GPIO 25 | Digital output |
| GND / - | GND | Common ground |

### Code reference

```cpp
static const int BUZZER_PIN = 25;
```

---

## 5. Boot Button / Sensitivity Toggle

The onboard ESP32 BOOT button is reused as the sensitivity toggle.

### Button mapping

| Function | ESP32 Pin | Notes |
|---|---:|---|
| Sensitivity toggle | GPIO 0 | Active LOW, internal pull-up |

### Code reference

```cpp
static const int BUTTON_PIN = 0;
pinMode(BUTTON_PIN, INPUT_PULLUP);
```

Pressing the button cycles:
- HIGH sensitivity
- MED sensitivity
- LOW sensitivity

---

## 6. Nokia 5110 LCD Wiring

The Nokia 5110 LCD is used for:
- state display
- distance display
- sensitivity label
- power bar

### Nokia 5110 -> ESP32

| LCD Pin | Connect To | Notes |
|---|---|---|
| CLK / SCLK | GPIO 18 | SPI clock |
| DIN / MOSI | GPIO 23 | SPI data |
| DC | GPIO 14 | Data/command |
| CS / SCE | GPIO 33 | Chip select |
| RST | GPIO 26 | Reset |
| VCC | 3.3V | Logic supply |
| GND | GND | Common ground |
| BL | 3.3V | Backlight, if used |

### Code reference

```cpp
static const int LCD5110_SCLK = 18;
static const int LCD5110_DIN  = 23;
static const int LCD5110_DC   = 14;
static const int LCD5110_CS   = 33;
static const int LCD5110_RST  = 26;
```

The current code also sets:

```cpp
display.setContrast(55);
display.setRotation(2);
```

So the screen is configured for upside-down mounting relative to default orientation.

---

## 7. Bluetooth / App Integration

The current Demo 3 code uses:

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
```

and starts the Bluetooth server as:

```cpp
SerialBT.begin("CyberFish_Rod");
```

### Important note

This is **ESP32 Classic Bluetooth Serial**, not an HC-05 module and not BLE in the current sketch. No external Bluetooth wiring is required.

The app data packet format is:

```text
forceHold,biteStatus,stateLabel,sensLabel,savedCastDistance
```

Example:

```text
0.42,FISH,WAIT,MED,4.8
```

---

## 8. Current State Machine Summary

The code uses three states:

- **ARMED**: waiting for cast onset from MPU6050 motion
- **CASTING**: tracking cast motion and power bar, resetting spool count at cast start
- **WAIT_BITE**: waiting for piezo-based bite detection after post-cast settling

### Motion source
- **MPU6050** handles cast detection and force-bar behavior

### Bite source
- **Piezo sensor** handles bite detection

### Distance source
- **A3144 hall sensor** counts magnet passes for line distance

---

## 9. Full Wiring Summary

### Power
- ESP32 `3.3V` -> MPU6050 VCC
- ESP32 `3.3V` -> A3144 VCC
- ESP32 `3.3V` -> Nokia 5110 VCC
- ESP32 `3.3V` -> Nokia 5110 BL (if backlight used)
- All module grounds tied together

### Signals
- GPIO 21 -> MPU6050 SDA
- GPIO 22 -> MPU6050 SCL
- GPIO 27 <- A3144 OUT
- GPIO 34 <- Piezo signal
- GPIO 25 -> Buzzer input
- GPIO 0 <- BOOT button
- GPIO 18 -> Nokia CLK
- GPIO 23 -> Nokia DIN
- GPIO 14 -> Nokia DC
- GPIO 33 -> Nokia CS
- GPIO 26 -> Nokia RST

---

## 10. Notes for Troubleshooting

### Hall sensor counts in standalone test but not in integrated code
This is usually not a pin issue. The likely causes are:
- hall debounce too large in integrated code
- distance display locked by state logic
- `savedCastDistance` only updating in selected states

### MPU6050 not found
Check:
- SDA on GPIO 21
- SCL on GPIO 22
- AD0 state vs I2C address
- shared ground

### Bluetooth app not connecting
Check that the phone/app is connecting to:

```text
CyberFish_Rod
```

and remember this sketch uses **Classic Bluetooth Serial**.

---

## 11. Code-Defined Pins Snapshot

```cpp
static const int MPU_SDA = 21;
static const int MPU_SCL = 22;
static const uint8_t MPU_ADDR = 0x68;

static const int HALL_PIN = 27;
static const int PIEZO_PIN = 34;
static const int BUZZER_PIN = 25;
static const int BUTTON_PIN = 0;

static const int LCD5110_SCLK = 18;
static const int LCD5110_DIN  = 23;
static const int LCD5110_DC   = 14;
static const int LCD5110_CS   = 33;
static const int LCD5110_RST  = 26;
```

