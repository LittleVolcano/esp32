# ESP32-C3 SuperMini MPU6050 Console

The firmware reads MPU6050 every 500 ms and prints acceleration, gyro, and temperature.

## Wiring

| ESP32-C3 SuperMini | MPU6050 |
| --- | --- |
| 3V3 | VCC |
| GND | GND |
| GPIO4 | SDA |
| GPIO5 | SCL |
| GND | AD0 |

Leave INT, XDA, and XCL unconnected. Do not use 5V or GPIO8.

## Build

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```
