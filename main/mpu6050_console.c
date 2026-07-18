#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 5
#define MPU6050_ADDRESS 0x68
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_WHO_AM_I_VALUE 0x68

static const char *TAG = "mpu6050";

static int16_t read_be16(const uint8_t *data) { return (int16_t)(((uint16_t)data[0] << 8) | data[1]); }

static void write_register(uint8_t reg, uint8_t value)
{
    const uint8_t message[] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, MPU6050_ADDRESS, message, sizeof(message), pdMS_TO_TICKS(100)));
}
static void read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    ESP_ERROR_CHECK(i2c_master_write_read_device(I2C_PORT, MPU6050_ADDRESS, &reg, 1, data, length, pdMS_TO_TICKS(100)));
}

static void initialize_i2c(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER, .sda_io_num = I2C_SDA_GPIO, .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, config.mode, 0, 0, 0));
}

static void initialize_mpu6050(void)
{
    uint8_t device_id = 0;
    read_registers(MPU6050_REG_WHO_AM_I, &device_id, 1);
    ESP_ERROR_CHECK(device_id == MPU6050_WHO_AM_I_VALUE ? ESP_OK : ESP_ERR_NOT_FOUND);
    write_register(MPU6050_REG_PWR_MGMT_1, 0x00);
    ESP_LOGI(TAG, "MPU6050 detected at 0x%02X", MPU6050_ADDRESS);
}

static void print_measurements(void)
{
    uint8_t data[14] = {0};
    read_registers(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));
    const float ax = read_be16(&data[0]) / 16384.0f, ay = read_be16(&data[2]) / 16384.0f, az = read_be16(&data[4]) / 16384.0f;
    const float temperature = read_be16(&data[6]) / 340.0f + 36.53f;
    const float gx = read_be16(&data[8]) / 131.0f, gy = read_be16(&data[10]) / 131.0f, gz = read_be16(&data[12]) / 131.0f;
    ESP_LOGI(TAG, "accel_g=(%.2f, %.2f, %.2f) gyro_dps=(%.2f, %.2f, %.2f) temperature_c=%.2f", ax, ay, az, gx, gy, gz, temperature);
}

void app_main(void)
{
    initialize_i2c();
    initialize_mpu6050();
    while (true) { print_measurements(); vTaskDelay(pdMS_TO_TICKS(500)); }
}
