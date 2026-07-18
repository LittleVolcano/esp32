from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main" / "mpu6050_console.c"


class Mpu6050SourceTests(unittest.TestCase):
    def test_configures_c3_supermini_i2c_and_mpu6050(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("#define I2C_SDA_GPIO 4", source)
        self.assertIn("#define I2C_SCL_GPIO 5", source)
        self.assertIn("#define MPU6050_ADDRESS 0x68", source)
        self.assertIn("MPU6050_REG_WHO_AM_I", source)
        self.assertIn("MPU6050_REG_PWR_MGMT_1", source)
        self.assertIn("pdMS_TO_TICKS(500)", source)

    def test_outputs_all_three_measurement_groups(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("accel_g=", source)
        self.assertIn("gyro_dps=", source)
        self.assertIn("temperature_c=", source)

    def test_displays_measurements_on_ssd1306_oled(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("#define OLED_ADDRESS_PRIMARY 0x3C", source)
        self.assertIn("#define OLED_ADDRESS_SECONDARY 0x3D", source)
        self.assertIn("#define OLED_HEIGHT 32", source)
        self.assertIn("initialize_oled();", source)
        self.assertIn("display_measurements(&measurements);", source)
        self.assertIn("if (oled_flush() != ESP_OK)", source)


if __name__ == "__main__":
    unittest.main()
