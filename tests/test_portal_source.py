from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PortalSourceTests(unittest.TestCase):
    def test_uses_sdkconfig_for_ssid_and_ap_address(self):
        kconfig = (ROOT / "main" / "Kconfig.projbuild").read_text(encoding="utf-8")
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn('config PORTAL_WIFI_SSID', kconfig)
        self.assertIn('default "ESP32-Portal"', kconfig)
        self.assertIn('config PORTAL_AP_IP_ADDRESS', kconfig)
        self.assertIn('default "192.168.4.1"', kconfig)
        self.assertIn('CONFIG_PORTAL_WIFI_SSID', source)
        self.assertIn('CONFIG_PORTAL_AP_IP_ADDRESS', source)

    def test_configures_open_portal_and_root_html_handler(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn('#define AP_SSID CONFIG_PORTAL_WIFI_SSID', source)
        self.assertIn('.authmode = WIFI_AUTH_OPEN', source)
        self.assertIn('.uri = "/"', source)
        self.assertIn('"text/html; charset=utf-8"', source)

    def test_logs_portal_startup_with_ssid_and_url(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        handler_registration = source.index(
            "ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));"
        )
        self.assertIn("ESP_LOGI(", source)
        startup_log = source.index("ESP_LOGI(", handler_registration)

        self.assertGreater(startup_log, handler_registration)
        self.assertIn("AP_SSID", source[startup_log:])
        self.assertIn("AP_IP_ADDRESS", source[startup_log:])


if __name__ == "__main__":
    unittest.main()
