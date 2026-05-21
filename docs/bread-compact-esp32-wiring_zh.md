# Bread Compact ESP32 接口说明

本文档对应板型：`main/boards/bread-compact-esp32`

当前硬件组合：

- 主控：ESP32 DevKit
- 麦克风：INMP441 I2S 数字麦克风
- 功放：MAX98357A I2S 数字功放
- 屏幕：SPI OLED SSD1306 128x32 或 128x64

## INMP441 麦克风

| INMP441 引脚 | ESP32 引脚 | 说明 |
| --- | --- | --- |
| `VDD` | `3V3` | 3.3V 供电，不要接 5V |
| `GND` | `GND` | 电源地 |
| `WS` | `GPIO19` | I2S 左右声道时钟 |
| `SCK` | `GPIO18` | I2S 位时钟 |
| `SD` | `GPIO21` | 麦克风数据输出到 ESP32 |
| `L/R` | `GND` | 选择左声道 |

对应代码：

```c
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_19
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_18
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_21
```

## MAX98357A 功放

| MAX98357A 引脚 | ESP32 / 外设连接 | 说明 |
| --- | --- | --- |
| `VIN` | `5V` 或 `3V3` | 按模块支持范围接入，常见模块可接 5V |
| `GND` | `GND` | 必须和 ESP32 共地 |
| `LRC` / `LRCK` | `GPIO13` | I2S 左右声道时钟 |
| `BCLK` | `GPIO12` | I2S 位时钟 |
| `DIN` | `GPIO14` | ESP32 音频数据输出到功放 |
| `GAIN` | 悬空 | 默认增益；声音太小可按模块资料调整 |
| `SD` | `VIN` 或悬空 | 功放使能；优先接 `VIN` 保持开启 |
| `SPK+` | 喇叭一端 | 喇叭输出正端 |
| `SPK-` | 喇叭另一端 | 喇叭输出负端 |

对应代码：

```c
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_14
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_13
```

注意：喇叭接在 `SPK+` 和 `SPK-` 之间，不要把喇叭一端接 `GND`。

## SPI OLED 屏幕

| OLED 引脚 | ESP32 引脚 | 说明 |
| --- | --- | --- |
| `VCC` | `3V3` | 屏幕供电 |
| `GND` | `GND` | 电源地 |
| `SCK` / `SCL` / `CLK` | `GPIO4` | SPI 时钟 |
| `SDA` / `MOSI` / `DIN` | `GPIO16` | SPI 数据 |
| `RES` / `RST` | `GPIO17` | 复位 |
| `DC` | `GPIO5` | 数据/命令选择 |
| `CS` | 不接 | 当前配置为 `GPIO_NUM_NC` |

对应代码：

```c
#define DISPLAY_SPI_SCLK_PIN GPIO_NUM_4
#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_16
#define DISPLAY_SPI_RST_PIN  GPIO_NUM_17
#define DISPLAY_SPI_DC_PIN   GPIO_NUM_5
#define DISPLAY_SPI_CS_PIN   GPIO_NUM_NC
```

## 其它引脚

| 功能 | ESP32 引脚 | 说明 |
| --- | --- | --- |
| BOOT 按钮 | `GPIO0` | 板载 BOOT 键 |
| 麦克风接收指示 LED | `GPIO2` | 当前按板载 LED 低电平点亮配置 |
| ASR 按钮 | 未使用 | 当前为 `GPIO_NUM_NC` |
| MCP 测试灯 | 未使用 | 当前为 `GPIO_NUM_NC` |

## 焊接检查清单

- INMP441 只能接 `3V3`，不要接 `5V`。
- MAX98357A、INMP441、OLED 的 `GND` 必须和 ESP32 `GND` 共地。
- MAX98357A 的 `DIN` 接 ESP32 的音频输出脚 `GPIO14`。
- INMP441 的 `SD` 接 ESP32 的音频输入脚 `GPIO21`。
- OLED 是 SPI 接法，不是只带 `SCL/SDA/VCC/GND` 的 I2C 接法。
- `GPIO0`、`GPIO2`、`GPIO12`、`GPIO15` 等属于 ESP32 启动相关敏感引脚，焊接外设时不要在上电瞬间强拉到错误电平。

## 清除 Wi-Fi 记忆

Wi-Fi 配网信息保存在 NVS 分区。当前 4MB 分区表中 NVS 位于：

```text
offset: 0x9000
size:   0x4000
```

测试时只擦除 NVS 即可让设备忘记已配对 Wi-Fi：

```powershell
C:\Espressif\tools\python\v6.0.1\venv\Scripts\esptool.exe --chip esp32 -p COM6 erase_region 0x9000 0x4000
```

擦除后按一下 ESP32 复位键，设备会重新进入配网流程。

如果串口监视器正在运行，需要先关闭 `monitor`，否则 `COM6` 会被占用。

## 音量设置

当前固件启动时会把软件输出音量设置为最大：

```c
#define AUDIO_DEFAULT_OUTPUT_VOLUME 100
```

这只影响 ESP32 输出到 MAX98357A 的数字音量。MAX98357A 的硬件增益仍由 `GAIN` 引脚决定；当前建议 `GAIN` 悬空。

## 麦克风指示灯

当前固件使用 `GPIO2` 作为“正在听用户说话”的状态指示。它跟随设备状态，而不是底层麦克风电源状态：

- OLED 显示 `listening`：`GPIO2` 输出低电平，板载 LED 亮。
- OLED 显示 `speaking` 或 `standby`：`GPIO2` 输出高电平，板载 LED 灭。

很多 ESP32 DevKit 的板载 `GPIO2` LED 是低电平点亮，所以当前配置为：

对应配置：

```c
#define MIC_ACTIVITY_LED_GPIO   BUILTIN_LED_GPIO
#define MIC_ACTIVITY_LED_ACTIVE_LEVEL 0
```

如果外接 LED 是高电平点亮，需要把 `MIC_ACTIVITY_LED_ACTIVE_LEVEL` 改为 `1`，并串联限流电阻。
