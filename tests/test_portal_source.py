from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SoftApSourceTests(unittest.TestCase):
    def test_matches_the_minimal_arduino_softap_configuration(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")

        self.assertIn('#define AP_SSID "ESP_AP"', source)
        self.assertIn('#define AP_PASSWORD "123456789"', source)
        self.assertIn('#define AP_CHANNEL 1', source)
        self.assertIn('.authmode = WIFI_AUTH_WPA2_PSK', source)
        self.assertIn('esp_wifi_set_mode(WIFI_MODE_AP)', source)
        self.assertIn('esp_wifi_set_config(WIFI_IF_AP, &ap_config)', source)
        self.assertIn('esp_wifi_start()', source)

    def test_serves_a_root_page_without_background_diagnostic_work(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")

        self.assertIn('esp_http_server.h', source)
        self.assertIn('httpd_start(&server, &server_config)', source)
        self.assertIn('httpd_register_uri_handler(server, &root_route)', source)
        self.assertIn('.uri = "/"', source)
        self.assertIn('"text/html; charset=utf-8"', source)
        self.assertIn('.uri = "/favicon.ico"', source)
        self.assertIn('httpd_resp_set_status(request, "204 No Content")', source)
        self.assertNotIn('xTaskCreate', source)
        self.assertNotIn('esp_netif_set_ip_info', source)

    def test_keeps_wifi_settings_in_ram_for_the_test(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")

        self.assertIn('esp_wifi_set_storage(WIFI_STORAGE_RAM)', source)


if __name__ == "__main__":
    unittest.main()
