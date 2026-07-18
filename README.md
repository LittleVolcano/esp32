# Audio-controlled motor PWM for ESP32-C3

本程序读取模拟麦克风模块的音频信号，用自相关法估算人声基频，并用 PWM 占空比控制电机驱动器：音调越高，占空比越大，电机转速越快。控制台会持续输出原始频率、平滑后的频率、声音幅度和 PWM 占空比。

## 接线

| 信号 | ESP32-C3 引脚 | 说明 |
| --- | --- | --- |
| 麦克风 AO（模拟输出） | GPIO0 / ADC1_CH0 | 模块必须以 3.3 V 供电，AO 电压不得超过 3.3 V。 |
| 麦克风 VCC / GND | 3.3 V / GND | 与 ESP32 共地。 |
| 电机驱动器 PWM / EN | GPIO6 | 只接电机驱动模块的逻辑输入。 |
| 电机驱动器 GND | GND | 必须与 ESP32 共地。 |

不要将电机直接接到 GPIO6。请使用 MOSFET、H 桥或带 PWM/EN 输入的电机驱动板，并按其说明添加续流二极管或使用板载保护。

## 控制逻辑

- 200 Hz 映射为 5% 占空比；
- 530 Hz 映射为 75% 占空比；
- 中间频率线性映射；
- 声音太小、没有稳定音调，或低于 200 Hz / 高于 530 Hz 时，输出 0% 占空比，电机停止。

控制区间为 200–530 Hz：200 Hz 对应 5%，530 Hz 对应 75%，其间线性映射。自相关测频比简单零交叉法更能抵抗人声的泛音；频率经 87.5/12.5 低通平滑后再控制 PWM，连续三帧无法识别前会暂时保持上一次转速，避免短暂误判造成电机停转。若电机在 5% 时无法起转，可在 `menuconfig` 中提高最小占空比。

这些阈值可以在 `idf.py menuconfig` 的 `Audio Motor PWM Configuration` 菜单中修改。

## 编译和监视

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

示例输出：

```text
I (...) audio_motor: raw_pitch=441 Hz, pitch=438 Hz, level=612, duty=389/1023 (38%)
```
