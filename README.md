# ESP32 SoftAP Web Portal

这是一个基于 ESP-IDF 的 ESP32-C3 SoftAP（无线接入点）和 Web 页面示例。

程序启动后会创建一个 Wi-Fi 接入点，并提供一个简单的 HTTP 页面：

- Wi-Fi 名称（SSID）：默认 `ESP32-Portal`，可在 SDK Configuration Editor 中配置
- 密码：无（开放网络）
- 页面地址：由配置的 AP IP 地址决定（默认 [http://192.168.4.1/](http://192.168.4.1/)）

## 配置 Portal 网络

在 VS Code 中打开命令面板，运行 **ESP-IDF: SDK Configuration Editor**，然后展开 **Portal Configuration**。修改以下配置项：

- **Wi-Fi name**：Portal 接入点的 SSID。
- **AP IP address**：Portal 接入点的 IP 地址。

保存配置后，必须重新构建并烧录固件，新的配置才会生效。连接到配置后的 Wi-Fi 网络后，在浏览器访问 `http://<AP IP address>/`；例如 AP IP address 为 `192.168.4.1` 时，访问 [http://192.168.4.1/](http://192.168.4.1/)。

## 使用方法

1. 在 **Portal Configuration** 中确认或修改 Wi-Fi name 和 AP IP address，然后重新编译并烧录固件到开发板。
2. 使用手机或电脑连接配置后的 Wi-Fi 网络（默认 `ESP32-Portal`），无需输入密码。
3. 打开浏览器，访问由配置的 AP IP address 派生的地址，例如 [http://192.168.4.1/](http://192.168.4.1/)。
4. 页面会显示“ESP32 Portal”和连接成功提示。

## 编译、烧录和监视

设置芯片目标后执行：

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为开发板对应的串口设备。串口波特率使用 `115200`。
