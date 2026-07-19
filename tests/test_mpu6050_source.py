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
        self.assertIn("pdMS_TO_TICKS(DISPLAY_REFRESH_MS)", source)

    def test_outputs_all_three_measurement_groups(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("accel_g=", source)
        self.assertIn("gyro_dps=", source)
        self.assertIn("temperature_c=", source)

    def test_displays_scalar_acceleration_change_on_ssd1306_oled(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("#define OLED_ADDRESS_PRIMARY 0x3C", source)
        self.assertIn("#define OLED_ADDRESS_SECONDARY 0x3D", source)
        self.assertIn("#define OLED_HEIGHT 32", source)
        self.assertIn("initialize_oled();", source)
        self.assertIn("display_measurements(&measurements);", source)
        self.assertIn("if (oled_flush() != ESP_OK)", source)
        self.assertIn("acceleration_change_magnitude", source)
        self.assertIn("sqrtf(", source)
        self.assertIn("oled_draw_accel_bar(acceleration_g, recent_peak_acceleration)", source)
        self.assertIn("#define ACCEL_BAR_X 4", source)
        self.assertIn("#define ACCEL_BAR_RANGE_G 1.5f", source)
        self.assertIn("#define CAT_TENSE_THRESHOLD_G 0.50f", source)
        self.assertIn("#define CAT_ANIMATION_FRAME_MS 200", source)
        self.assertIn("oled_draw_cat(peak_acceleration_g)", source)
        self.assertIn("oled_draw_sprite", source)
        self.assertIn("reference_lying_cat", source)
        self.assertIn("rasterized from the user-provided reference image", source)
        self.assertIn("cat_animation_frame", source)
        self.assertIn("#define ACCEL_DEADBAND_G 0.02f", source)
        self.assertIn("#define INITIAL_BASELINE_SAMPLES 20", source)
        self.assertIn("initial_magnitude_g", source)
        self.assertIn("fabsf(total_magnitude_g - initial_magnitude_g)", source)
        self.assertIn("#define DYNAMIC_FILTER_ALPHA 0.75f", source)
        self.assertIn("#define MOTION_CONFIRM_DURATION_MS 200", source)
        self.assertIn("filter_acceleration_change", source)
        self.assertIn("dynamic_motion_confirmed", source)
        self.assertIn("calculate_acceleration_change", source)
        self.assertNotIn("remove_initial_gravity_axis", source)
        self.assertNotIn("predict_gravity_from_gyro", source)
        self.assertIn("#define PEAK_FLASH_THRESHOLD_G 1.0f", source)
        self.assertIn("#define PEAK_FLASH_DURATION_MS 500", source)
        self.assertIn("oled_set_inverted", source)
        self.assertIn("acceleration_g > recent_peak_acceleration", source)
        self.assertNotIn("bar_centre", source)
        self.assertIn('snprintf(value_text, sizeof(value_text), "%.1fG", acceleration_g)', source)
        self.assertIn('snprintf(peak_text, sizeof(peak_text), "%.1fG", peak_acceleration_g)', source)
        self.assertIn("#define DISPLAY_REFRESH_MS 100", source)
        self.assertIn("#define PEAK_HISTORY_DURATION_MS 5000", source)
        self.assertIn("update_recent_acceleration_peak", source)


if __name__ == "__main__":
    unittest.main()
