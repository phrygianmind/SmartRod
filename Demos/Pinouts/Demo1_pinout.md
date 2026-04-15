# Smart Rod Demo 1 — Full Pinout (ESP32 DevKit V1)

## ESP32 DevKit V1
- **3V3**: powers TCS34725 + BNO085  
- **5V / VIN**: powers LCD1602 VDD  
- **GND**: common ground for all modules  

---

## TCS34725 Color Sensor (I2C)
- **VIN** → ESP32 **3V3**
- **GND** → ESP32 **GND**
- **SDA** → ESP32 **GPIO21**
- **SCL** → ESP32 **GPIO22**

---

## BNO085 IMU (UART-RVC)
> UART-RVC uses the BNO’s **SDA pin as UART TX output**.

- **VIN** → ESP32 **3V3**
- **GND** → ESP32 **GND**
- **SDA (UART OUT)** → ESP32 **GPIO19** (UART2 RX)
- **P0** → ESP32 **3V3** 
- **SCL** → **NC** (not used)
- ESP32 UART TX → **NC** (not used)

---

## LCD1602 (HD44780, 4-bit parallel)

### Power / contrast / backlight
- **Pin 1 (VSS)** → ESP32 **GND**
- **Pin 2 (VDD)** → ESP32 **5V / VIN** *(recommended)*
- **Pin 3 (VO)** → **GND** *(or pot wiper for adjustable contrast)*
- **Pin 15 (A / LED+)** → **3V3** *(optional series resistor 220Ω–1kΩ if too bright)*
- **Pin 16 (K / LED-)** → **GND**

### Control + data 
- **Pin 4 (RS)** → ESP32 **GPIO14**
- **Pin 5 (RW)** → **GND**
- **Pin 6 (E)** → ESP32 **GPIO27**
- **Pin 11 (D4)** → ESP32 **GPIO26**
- **Pin 12 (D5)** → ESP32 **GPIO25**
- **Pin 13 (D6)** → ESP32 **GPIO33**
- **Pin 14 (D7)** → ESP32 **GPIO32**

### Unused (leave disconnected)
- **Pin 7 (D0)** → NC  
- **Pin 8 (D1)** → NC  
- **Pin 9 (D2)** → NC  
- **Pin 10 (D3)** → NC  

---

## Notes
- All grounds must be shared (common GND rail).
- Do **not** connect the BNO085 to the I2C bus when using UART-RVC (only VIN/GND + SDA→GPIO19 + P0 strap).
