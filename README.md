# ESP32-C3 SoftAP HTTP Test

This branch is an A/B test for unstable SoftAP discovery. It contains only the
ESP-IDF Wi-Fi AP startup path and matches the supplied Arduino example.

- SSID: `ESP_AP`
- Password: `123456789`
- Security: WPA2-PSK
- Channel: 1
- AP address: `192.168.4.1` (ESP-IDF default)

The program starts a minimal HTTP server after the SoftAP is ready. Connect to
`ESP_AP`, then open [http://192.168.4.1/](http://192.168.4.1/). There is no
custom IP configuration, background task, or periodic status logging. After
boot, the expected console messages are:

```text
I (...) softap_test: SoftAP started: SSID=ESP_AP channel=1 password=123456789
I (...) softap_test: HTTP server ready at http://192.168.4.1/
```

Build and flash:

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the board serial device.
