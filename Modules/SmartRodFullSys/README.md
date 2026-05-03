# SmartRod Full System - Refactored

Open `SmartRod_Full_System_Refactored.ino` in Arduino IDE. Keep all `.h` files in the same sketch folder.

Main changes:

- Split the original single-file sketch into small feature headers.
- Kept the same state machine and hardware behavior.
- Replaced Bluetooth `String` packet building with a fixed char buffer to reduce heap fragmentation.
- Moved repeated setup/loop blocks into named helper functions for easier debugging.

File map:

- `SmartRod_Config.h` - libraries, pins, constants, thresholds.
- `SmartRod_Globals.h` - hardware objects and runtime state variables.
- `SmartRod_StateMachine.h` - ARMED, CASTING, WAIT_BITE, REELING transitions.
- `SmartRod_IMU.h` - MPU6050 reading and force peak hold.
- `SmartRod_Piezo.h` - piezo baseline and bite detection.
- `SmartRod_Hall.h` - hall ISR and pulse consumption.
- `SmartRod_Distance.h` - line distance calculation.
- `SmartRod_Display.h` - Nokia 5110 display UI.
- `SmartRod_Bluetooth.h` - app telemetry packet sending.
- `SmartRod_Button.h` - sensitivity button handling.
- `SmartRod_Buzzer.h` - bite alert buzzer helpers.
- `SmartRod_Commands.h` - serial/Bluetooth command handling.
- `SmartRod_Labels.h` - small text-label helpers.
