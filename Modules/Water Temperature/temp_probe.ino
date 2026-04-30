#include <OneWire.h>
#include <DallasTemperature.h>

// DS18B20 data pin
static const int ONE_WIRE_BUS = 4;

// Set up OneWire and DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("DS18B20 Temperature Test");
  Serial.println("------------------------");

  sensors.begin();

  // Check how many DS18B20 sensors are found
  int deviceCount = sensors.getDeviceCount();
  Serial.print("Devices found: ");
  Serial.println(deviceCount);

  if (deviceCount == 0) {
    Serial.println("No DS18B20 detected. Check wiring and pull-up resistor.");
  }
}

void loop() {
  // Request temperature conversion
  sensors.requestTemperatures();

  float tempC = sensors.getTempCByIndex(0);
  float tempF = DallasTemperature::toFahrenheit(tempC);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor disconnected or not detected.");
  } else {
    Serial.print("Temperature: ");
    Serial.print(tempC, 2);
    Serial.print(" C  |  ");
    Serial.print(tempF, 2);
    Serial.println(" F");
  }

  delay(1000);
}
