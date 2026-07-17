# ESP32 Wi-Fi RSSI Scanner

这是一个基于 ESP-IDF 的 ESP32-C3 Wi-Fi 扫描程序。

程序启动后扫描所有 Wi-Fi 信道，获取附近的接入点，并按照 RSSI（信号强度）从强到弱排序，在串口输出：

- RSSI 信号强度
- Wi-Fi 信道
- 加密方式
- SSID

程序只扫描，不连接任何 Wi-Fi，也不需要配置 SSID 或密码。

## 编译、烧录和监视

设置芯片目标后执行：

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为开发板对应的串口设备。串口波特率使用 `115200`。

输出示例：

```text
No.  RSSI   CH  AUTH        SSID
---- ----- --- ----------- ------------------------------
1      -42   6 WPA2        MyWifi
2      -58  11 WPA/WPA2    Office
3      -73   1 OPEN        Guest
```
