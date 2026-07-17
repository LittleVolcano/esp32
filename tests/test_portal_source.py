from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PortalSourceTests(unittest.TestCase):
    def test_configures_open_portal_and_root_html_handler(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn('#define AP_SSID "ESP32-Portal"', source)
        self.assertIn('.authmode = WIFI_AUTH_OPEN', source)
        self.assertIn('.uri = "/"', source)
        self.assertIn('"text/html; charset=utf-8"', source)


if __name__ == "__main__":
    unittest.main()
