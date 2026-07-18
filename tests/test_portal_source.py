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
        self.assertIn('.ssid_hidden = 0', source)
        self.assertIn('esp_wifi_set_max_tx_power', source)
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
        self.assertIn(
            'ESP_LOGI(TAG, "SoftAP %s ready at http://%s/", AP_SSID, AP_IP_ADDRESS);',
            source,
        )

    def test_uses_compatible_lwip_addresses_for_ap_network(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn("ip4_addr_t parsed_ip", source)
        self.assertIn("ip4addr_aton(AP_IP_ADDRESS, &parsed_ip)", source)
        self.assertIn("ip_info.ip.addr = parsed_ip.addr", source)
        self.assertIn("ip_info.gw.addr = parsed_ip.addr", source)
        self.assertIn("ip4_addr_t netmask", source)
        self.assertIn("IP4_ADDR(&netmask, 255, 255, 255, 0)", source)
        self.assertIn("ip_info.netmask.addr = netmask.addr", source)
        self.assertNotIn("ip4addr_aton(AP_IP_ADDRESS, &ip_info.ip)", source)

    def test_logs_ap_events_and_periodic_status(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn("WIFI_EVENT_AP_START", source)
        self.assertIn("WIFI_EVENT_AP_STOP", source)
        self.assertIn("AP heartbeat", source)
        self.assertIn("esp_wifi_get_max_tx_power", source)


if __name__ == "__main__":
    unittest.main()
