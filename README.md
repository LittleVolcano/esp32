# ESP32 SoftAP Web Portal

这是一个基于 ESP-IDF 的 ESP32-C3 SoftAP（无线接入点）和 Web 页面示例。

程序启动后会创建一个 Wi-Fi 接入点，并提供一个简单的 HTTP 页面：

- Wi-Fi 名称（SSID）：`ESP32-Portal`
- 密码：无（开放网络）
- 页面地址：[http://192.168.4.1/](http://192.168.4.1/)

## 使用方法

1. 编译并烧录固件到开发板。
2. 使用手机或电脑连接 Wi-Fi 网络 `ESP32-Portal`，无需输入密码。
3. 打开浏览器，访问 [http://192.168.4.1/](http://192.168.4.1/)。
4. 页面会显示“ESP32 Portal”和连接成功提示。

## 编译、烧录和监视

设置芯片目标后执行：

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为开发板对应的串口设备。串口波特率使用 `115200`。
