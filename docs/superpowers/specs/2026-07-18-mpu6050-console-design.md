# MPU6050 Console Design

## Goal

Run an MPU6050 on an ESP32-C3 SuperMini and print acceleration, angular velocity, and temperature every 500 ms.

## Wiring

- 3V3 to VCC
- GND to GND and AD0
- GPIO4 to SDA
- GPIO5 to SCL
- INT, XDA, and XCL unconnected

## Firmware

The firmware initializes I2C master at 400 kHz, checks WHO_AM_I at address 0x68, clears PWR_MGMT_1 to wake the sensor, and reads the 14-byte measurement block. It converts acceleration to g, gyro to degrees per second, and temperature to degrees Celsius.

Any I2C error or unexpected device ID stops startup with ESP_ERROR_CHECK. The current SoftAP and HTTP server are removed.

