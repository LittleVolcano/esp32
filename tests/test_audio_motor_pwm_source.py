from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main" / "audio_motor_pwm.c"


class AudioMotorPwmSourceTests(unittest.TestCase):
    def test_uses_analog_microphone_and_motor_pwm_pins(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("#define MICROPHONE_ADC_CHANNEL       ADC_CHANNEL_0", source)
        self.assertIn("#define MOTOR_PWM_GPIO               6", source)
        self.assertIn("adc_oneshot_read", source)
        self.assertIn("ledc_set_duty", source)

    def test_maps_detected_frequency_to_duty_and_reports_it(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("duty_for_frequency", source)
        self.assertIn("MIN_AUTOCORRELATION_PERCENT", source)
        self.assertIn("detected_lag", source)
        self.assertIn("raw_pitch=%", source)
        self.assertIn("duty=%", source)


if __name__ == "__main__":
    unittest.main()
