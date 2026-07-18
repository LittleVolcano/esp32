# ESP32-C3 SuperMini MPU6050 with OLED

The firmware reads MPU6050 every 500 ms, prints acceleration, gyro, and temperature to the serial console, and displays the same values on a 0.91 inch SSD1306 I2C OLED.

## Wiring

| ESP32-C3 SuperMini | MPU6050 | 0.91 inch OLED |
| --- | --- | --- |
| 3V3 | VCC | VCC |
| GND | GND | GND |
| GPIO4 | SDA | SDA |
| GPIO5 | SCL | SCL |
| GND | AD0 | - |

MPU6050 and OLED share the same I2C bus. Leave MPU6050 INT, XDA, and XCL unconnected. Use 3.3 V for both modules; do not use 5 V or GPIO8.

The program supports a common 128x32 SSD1306 OLED at I2C address `0x3C` or `0x3D`, and automatically detects either address. If neither address responds, the MPU6050 console output remains active and a warning is printed instead of restarting. Check the controller/address printed on the module before changing the SSD1306 initialization sequence in `main/mpu6050_console.c`.

## Build

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```
